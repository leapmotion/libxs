/*
    Copyright (c) 2009-2012 250bpm s.r.o.
    Copyright (c) 2007-2011 iMatix Corporation
    Copyright (c) 2007-2011 Other contributors as noted in the AUTHORS file

    This file is part of Crossroads I/O project.

    Crossroads I/O is free software; you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Crossroads is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "platform.hpp"
#if defined XS_HAVE_WINDOWS
#include "windows.hpp"
#else
#   if HAVE_UNISTD_H
#       include <unistd.h>
#   endif
#   if HAVE_SYS_TYPES_H
#       include <sys/types.h>
#   endif
#   if HAVE_SYS_STAT_H
#       include <sys/stat.h>
#   endif
#   if HAVE_DLFCN_H
#       include <dlfcn.h>
#   endif
#   if HAVE_LINK_H
#       include <link.h>
#   endif
#   if HAVE_FCNTL_H
#       include <fcntl.h>
#   endif
#   if HAVE_DIRENT_H
#       include <dirent.h>
#   endif
#endif

#include <new>
#include <string.h>

#include "ctx.hpp"
#include "socket_base.hpp"
#include "reaper.hpp"
#include "pipe.hpp"
#include "err.hpp"
#include "msg.hpp"
#include "prefix_filter.hpp"
#include "topic_filter.hpp"

xs::ctx_t::ctx_t () :
    tag (0xbadcafe0),
    starting (true),
    terminating (false),
    reaper (NULL),
    slot_count (0),
    slots (NULL),
    max_sockets (512),
    io_thread_count (1)
{
    int rc = mailbox_init (&term_mailbox);
    errno_assert (rc == 0);

    //  Plug in the standard plugins.
    rc = plug (prefix_filter);
    errno_assert (rc == 0);
    rc = plug (topic_filter);
    errno_assert (rc == 0);

    //  Now plug in all the extensions found in plugin directory.

#if defined XS_HAVE_WINDOWS

    //  Find out the path to the plugins.
    char files_common [MAX_PATH];
    HRESULT hr = SHGetFolderPath (NULL, CSIDL_PROGRAM_FILES_COMMON,
        NULL, SHGFP_TYPE_CURRENT, files_common);
    xs_assert (hr == S_OK);
    std::string path = files_common;
    WIN32_FIND_DATA ffd;
    HANDLE fh = FindFirstFile ((path + "\\xs\\plugins\\*").c_str (), &ffd);
    if (fh == INVALID_HANDLE_VALUE)
        return;
    while (true) {

        //  Ignore the files without .xsp extension.
        std::string file = ffd.cFileName;
        if (file.size () < 4)
            goto next;
        if (file.substr (file.size () - 4) != ".xsp")
            goto next;

        //  Load the library and locate the extension point.
        HMODULE dl = LoadLibrary ((path + "\\xs\\" + ffd.cFileName).c_str ());
        if (!dl)
            goto next;
        file = std::string ("xsp_") + file.substr (0, file.size () - 4) +
            "_init";
        void *(*initfn) ();
        *(void**)(&initfn) = GetProcAddress (dl, file.c_str ());
        if (!initfn)
            goto next;

        //  Plug the extension into the context.
        rc = plug (initfn ());
        if (rc != 0) {
            FreeLibrary (dl);
            goto next;
        }

        //  Store the library handle so that we can unload it
        //  when context is terminated.
        opt_sync.lock ();
        plugins.push_back (dl);
        opt_sync.unlock ();

next:
        if (FindNextFile (fh, &ffd) == 0)
            break;
    }
    BOOL brc = FindClose (fh);
    win_assert (brc != 0);

#elif XS_HAVE_PLUGINS

    //  Load all the installed plug-ins.
    std::string path (XS_LIBDIR_PATH);
    path += "/xs/plugins";

    DIR *dp = opendir (path.c_str ());
    if (!dp && (errno == ENOENT || errno == EACCES || errno == ENOTDIR))
        return;
    errno_assert (dp);

    dirent dir, *dirp;
    struct stat stats;

    while (true) {
        rc = readdir_r (dp, &dir, &dirp);
        assert (rc == 0);
        if (!dirp)
            break;

        rc = lstat (dirp->d_name, &stats);

        if ((rc == 0) && S_ISREG (stats.st_mode)) {

            //  Ignore the files without .xsp extension.
            std::string file = dir.d_name;
            if (file.size () < 4)
                continue;
            if (file.substr (file.size () - 4) != ".xsp")
                continue;

            //  Open the specified dynamic library.
            std::string filename = path + "/" + file;
            void *dl = dlopen (filename.c_str (), RTLD_LOCAL | RTLD_NOW);
            if (!dl)
                continue;

            //  Find the initial entry point in the library. void pointer
            //  returned by dlsym is converted to function pointer using
            //  "union cast". There's no other way to do that as POSIX and
            //  C++ standards collide at this spot.
            file = std::string ("xsp_") + file.substr (0, file.size () - 4) +
                "_init";
            dlerror ();
            union {
                void *pdata;
                void *(*pfunc) ();
            } initfn;
            initfn.pdata = dlsym (dl, file.c_str ());
            if (!initfn.pdata)
                continue;

            //  Plug the extension into the context.
            int rc = plug (initfn.pfunc ());
            if (rc != 0) {
                dlclose (dl);
                continue;
            }

            //  Store the library handle so that we can unload it
            //  when context is terminated.
            opt_sync.lock ();
            plugins.push_back (dl);
            opt_sync.unlock ();
        }
    }
    closedir (dp);

#endif
}

bool xs::ctx_t::check_tag ()
{
    return tag == 0xbadcafe0;
}

xs::ctx_t::~ctx_t ()
{
    //  Check that there are no remaining sockets.
    xs_assert (sockets.empty ());

    //  Ask I/O threads to terminate. If stop signal wasn't sent to I/O
    //  thread subsequent invocation of destructor would hang-up.
    for (io_threads_t::size_type i = 0; i != io_threads.size (); i++)
        io_threads [i]->stop ();

    //  Wait till I/O threads actually terminate.
    for (io_threads_t::size_type i = 0; i != io_threads.size (); i++)
        delete io_threads [i];

    //  Deallocate the reaper thread object.
    if (reaper)
        delete reaper;

    //  Deallocate the array of mailboxes. No special work is
    //  needed as mailboxes themselves were deallocated with their
    //  corresponding io_thread/socket objects.
    if (slots)
        free (slots);

    //  Deallocate the termination mailbox.
    mailbox_close (&term_mailbox);

    //  Remove the tag, so that the object is considered dead.
    tag = 0xdeadbeef;
}

int xs::ctx_t::terminate ()
{
    slot_sync.lock ();
    if (!starting) {

        //  Check whether termination was already underway, but interrupted and now
        //  restarted.
        bool restarted = terminating;
        terminating = true;
        slot_sync.unlock ();

        //  First attempt to terminate the context.
        if (!restarted) {

            //  First send stop command to sockets so that any blocking calls
            //  can be interrupted. If there are no sockets we can ask reaper
            //  thread to stop.
            slot_sync.lock ();
            for (sockets_t::size_type i = 0; i != sockets.size (); i++)
                sockets [i]->stop ();
            if (sockets.empty ())
                reaper->stop ();
            slot_sync.unlock ();
        }

        //  Wait till reaper thread closes all the sockets.
        command_t cmd;
        int rc = mailbox_recv (&term_mailbox, &cmd, -1);
        if (rc == -1 && errno == EINTR)
            return -1;
        errno_assert (rc == 0);
        xs_assert (cmd.type == command_t::done);
        slot_sync.lock ();
        xs_assert (sockets.empty ());
    }
    slot_sync.unlock ();

    //  Unload any dynamically loaded extension libraries.
#if defined XS_HAVE_WINDOWS
    opt_sync.lock ();
    for (plugins_t::iterator it = plugins.begin ();
          it != plugins.end (); ++it)
        FreeLibrary (*it);
    opt_sync.unlock ();
#elif XS_HAVE_PLUGINS
    opt_sync.lock ();
    for (plugins_t::iterator it = plugins.begin ();
          it != plugins.end (); ++it)
        dlclose (*it);
    opt_sync.unlock ();
#endif

    //  Deallocate the resources.
    delete this;

    return 0;
}

int xs::ctx_t::plug (const void *ext_)
{
    if (!ext_) {
        errno = EFAULT;
        return -1;
    }

    //  The extension is a message filter plug-in.
    xs_filter_t *filter = (xs_filter_t*) ext_;
    if (filter->type == XS_PLUGIN_FILTER && filter->version == 1) {
       opt_sync.lock ();
       filters [filter->id (NULL)] = filter;
       opt_sync.unlock ();
       return 0;
    }

    //  Specified extension type is not supported by this version of
    //  the library.
    errno = ENOTSUP;
    return -1;
}

int xs::ctx_t::setctxopt (int option_, const void *optval_, size_t optvallen_)
{
    switch (option_) {
    case XS_MAX_SOCKETS:
        if (optvallen_ != sizeof (int) || *((int*) optval_) < 0) {
            errno = EINVAL;
            return -1;
        }
        opt_sync.lock ();
        max_sockets = *((int*) optval_);
        opt_sync.unlock ();
        break;
    case XS_IO_THREADS:
        if (optvallen_ != sizeof (int) || *((int*) optval_) < 1) {
            errno = EINVAL;
            return -1;
        }
        opt_sync.lock ();
        io_thread_count = *((int*) optval_);
        opt_sync.unlock ();
        break;
    case XS_PLUGIN:
        return plug (optval_);
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

xs::socket_base_t *xs::ctx_t::create_socket (int type_)
{
    slot_sync.lock ();
    if (unlikely (starting)) {

        starting = false;

        //  Initialise the array of mailboxes. Additional three slots are for
        //  xs_term thread and reaper thread.
        opt_sync.lock ();
        int maxs = max_sockets;
        int ios = io_thread_count;
        opt_sync.unlock ();
        slot_count = maxs + ios + 2;
        slots = (mailbox_t**) malloc (sizeof (mailbox_t*) * slot_count);
        alloc_assert (slots);

        //  Initialise the infrastructure for xs_term thread.
        slots [term_tid] = &term_mailbox;

        //  Create the reaper thread.
        reaper = new (std::nothrow) reaper_t (this, reaper_tid);
        alloc_assert (reaper);
        slots [reaper_tid] = reaper->get_mailbox ();
        reaper->start ();

        //  Create I/O thread objects and launch them.
        for (int i = 2; i != ios + 2; i++) {
            io_thread_t *io_thread = io_thread_t::create (this, i);
            errno_assert (io_thread);
            io_threads.push_back (io_thread);
            slots [i] = io_thread->get_mailbox ();
            io_thread->start ();
        }

        //  In the unused part of the slot array, create a list of empty slots.
        for (int32_t i = (int32_t) slot_count - 1;
              i >= (int32_t) ios + 2; i--) {
            empty_slots.push_back (i);
            slots [i] = NULL;
        }
    }

    //  Once xs_term() was called, we can't create new sockets.
    if (terminating) {
        slot_sync.unlock ();
        errno = ETERM;
        return NULL;
    }

    //  If max_sockets limit was reached, return error.
    if (empty_slots.empty ()) {
        slot_sync.unlock ();
        errno = EMFILE;
        return NULL;
    }

    //  Choose a slot for the socket.
    uint32_t slot = empty_slots.back ();
    empty_slots.pop_back ();

    //  Generate new unique socket ID.
    int sid = ((int) max_socket_id.add (1)) + 1;

    //  Create the socket and register its mailbox.
    socket_base_t *s = socket_base_t::create (type_, this, slot, sid);
    if (!s) {
        empty_slots.push_back (slot);
        slot_sync.unlock ();
        return NULL;
    }
    sockets.push_back (s);
    slots [slot] = s->get_mailbox ();

    slot_sync.unlock ();

    return s;
}

void xs::ctx_t::destroy_socket (class socket_base_t *socket_)
{
    slot_sync.lock ();

    //  Free the associared thread slot.
    uint32_t tid = socket_->get_tid ();
    empty_slots.push_back (tid);
    slots [tid] = NULL;

    //  Remove the socket from the list of sockets.
    sockets.erase (socket_);

    //  If xs_term() was already called and there are no more socket
    //  we can ask reaper thread to terminate.
    if (terminating && sockets.empty ())
        reaper->stop ();

    slot_sync.unlock ();
}

xs::object_t *xs::ctx_t::get_reaper ()
{
    return reaper;
}

xs_filter_t *xs::ctx_t::get_filter (int filter_id_)
{
    xs_filter_t *result = NULL;
    opt_sync.lock ();
    filters_t::iterator it = filters.find (filter_id_);
    if (it != filters.end ())
        result = it->second;
    opt_sync.unlock ();
    return result;
}

void xs::ctx_t::send_command (uint32_t tid_, const command_t &command_)
{
    mailbox_send (slots [tid_], command_);
}

xs::io_thread_t *xs::ctx_t::choose_io_thread (uint64_t affinity_)
{
    if (io_threads.empty ())
        return NULL;

    //  Find the I/O thread with minimum load.
    int min_load = -1;
    io_threads_t::size_type result = 0;
    for (io_threads_t::size_type i = 0; i != io_threads.size (); i++) {
        if (!affinity_ || (affinity_ & (uint64_t (1) << i))) {
            int load = io_threads [i]->get_load ();
            if (min_load == -1 || load < min_load) {
                min_load = load;
                result = i;
            }
        }
    }
    xs_assert (min_load != -1);
    return io_threads [result];
}

int xs::ctx_t::register_endpoint (const char *addr_, endpoint_t &endpoint_)
{
    endpoints_sync.lock ();

    bool inserted = endpoints.insert (endpoints_t::value_type (
        std::string (addr_), endpoint_)).second;
    if (!inserted) {
        errno = EADDRINUSE;
        endpoints_sync.unlock ();
        return -1;
    }

    endpoints_sync.unlock ();
    return 0;
}

void xs::ctx_t::unregister_endpoints (socket_base_t *socket_)
{
    endpoints_sync.lock ();

    endpoints_t::iterator it = endpoints.begin ();
    while (it != endpoints.end ()) {
        if (it->second.socket == socket_) {
            endpoints_t::iterator to_erase = it;
            ++it;
            endpoints.erase (to_erase);
            continue;
        }
        ++it;
    }

    endpoints_sync.unlock ();
}

xs::endpoint_t xs::ctx_t::find_endpoint (const char *addr_)
{
     endpoints_sync.lock ();

     endpoints_t::iterator it = endpoints.find (addr_);
     if (it == endpoints.end ()) {
         endpoints_sync.unlock ();
         errno = ECONNREFUSED;
         endpoint_t empty = {NULL, options_t()};
         return empty;
     }
     endpoint_t endpoint = it->second;

     //  Increment the command sequence number of the peer so that it won't
     //  get deallocated until "bind" command is issued by the caller.
     //  The subsequent 'bind' has to be called with inc_seqnum parameter
     //  set to false, so that the seqnum isn't incremented twice.
     endpoint.socket->inc_seqnum ();

     endpoints_sync.unlock ();
     return endpoint;
}

//  The last used socket ID, or 0 if no socket was used so far. Note that this
//  is a global variable. Thus, even sockets created in different contexts have
//  unique IDs.
xs::atomic_counter_t xs::ctx_t::max_socket_id;
