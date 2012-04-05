/*
    Copyright (c) 2010-2012 250bpm s.r.o.
    Copyright (c) 2011 VMware, Inc.
    Copyright (c) 2010-2011 Other contributors as noted in the AUTHORS file

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

#include <string.h>

#include "xsub.hpp"
#include "err.hpp"
#include "wire.hpp"

xs::xsub_t::xsub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_),
    has_message (false),
    more (false),
    tmp_pipe (NULL),
    tmp_filter_id (-1)
{
    options.type = XS_XSUB;

    //  When socket is being closed down we don't want to wait till pending
    //  subscription commands are sent to the wire.
    options.linger = 0;

    //  Also, we want the subscription buffer to be elastic by default.
    options.sndhwm = 0;

    int rc = message.init ();
    errno_assert (rc == 0);
}

xs::xsub_t::~xsub_t ()
{
    //  Deallocate all the filters.
    for (filters_t::iterator it = filters.begin (); it != filters.end (); ++it)
        it->type->sf_destroy ((void*) (core_t*) this, it->instance);

    int rc = message.close ();
    errno_assert (rc == 0);
}

void xs::xsub_t::xattach_pipe (pipe_t *pipe_, bool icanhasall_)
{
    xs_assert (pipe_);
    fq.attach (pipe_);

    //  Pipes with 0MQ/2.1-style protocol are not eligible for accepting
    //  subscriptions.
    if (pipe_->get_protocol () != 1)
        dist.attach (pipe_);

    //  Send all the cached subscriptions to the new upstream peer.
    tmp_pipe = pipe_;
    for (filters_t::iterator it = filters.begin (); it != filters.end ();
          ++it) {
        tmp_filter_id = it->type->id (NULL);
        it->type->sf_enumerate ((void*) (core_t*) this, it->instance);
        tmp_filter_id = -1;
    }
    pipe_->flush ();
    tmp_pipe = NULL;
}

void xs::xsub_t::xread_activated (pipe_t *pipe_)
{
    fq.activated (pipe_);
}

void xs::xsub_t::xwrite_activated (pipe_t *pipe_)
{
    dist.activated (pipe_);
}

void xs::xsub_t::xterminated (pipe_t *pipe_)
{
    fq.terminated (pipe_);
    if (pipe_->get_protocol () != 1)
        dist.terminated (pipe_);
}

void xs::xsub_t::xhiccuped (pipe_t *pipe_)
{
    //  Send all the cached subscriptions to the hiccuped pipe.
    if (pipe_->get_protocol () != 1) {
        tmp_pipe = pipe_;
        for (filters_t::iterator it = filters.begin (); it != filters.end ();
              ++it) {
            tmp_filter_id = it->type->id (NULL);
            it->type->sf_enumerate ((void*) (core_t*) this, it->instance);
            tmp_filter_id = -1;
        }
        pipe_->flush ();
        tmp_pipe = NULL;
    }
}

int xs::xsub_t::xsend (msg_t *msg_, int flags_)
{
    size_t size = msg_->size ();
    unsigned char *data = (unsigned char*) msg_->data ();

    if (size < 4) {
        errno = EINVAL;
        return -1;
    }
    int cmd = get_uint16 (data);
    int filter_id = get_uint16 (data + 2);

#if 0
    //  TODO: This is 0MQ/3.1 protocol.
    if (size < 1) {
        errno = EINVAL;
        return -1;
    }
    int cmd = data [0] ? XS_CMD_SUBSCRIBE : XS_CMD_UNSUBSCRIBE;
    int filter_id = XS_FILTER_PREFIX;
#endif

    if (cmd != XS_CMD_SUBSCRIBE && cmd != XS_CMD_UNSUBSCRIBE) {
        errno = EINVAL;
        return -1;
    }
    
    //  Find the relevant filter.
    filters_t::iterator it;
    for (it = filters.begin (); it != filters.end (); ++it)
        if (it->type->id (NULL) == filter_id)
            break;

    //  Process the subscription.
    if (cmd == XS_CMD_SUBSCRIBE) {

        //  If the filter of the specified type does not exist yet, create it.
        if (it == filters.end ()) {
            filter_t f;
            f.type = get_filter (filter_id);
            xs_assert (f.type);
            f.instance = f.type->sf_create ((void*) (core_t*) this);
            xs_assert (f.instance);
            filters.push_back (f);
            it = filters.end () - 1;
        }

        if (it->type->sf_subscribe ((void*) (core_t*) this,
              it->instance, data + 4, size - 4) == 1)
#if 0
        //  TODO: This is 0MQ/3.1 protocol.
        if (it->type->sf_subscribe ((void*) (core_t*) this,
              it->instance, data + 1, size - 1) == 1)
#endif
            return dist.send_to_all (msg_, flags_);
        else
            return 0;
    }
    else if (cmd == XS_CMD_UNSUBSCRIBE) {
        xs_assert (it != filters.end ());

        if (it->type->sf_unsubscribe ((void*) (core_t*) this,
              it->instance, data + 4, size - 4) == 1)
#if 0
        //  TODO: This is 0MQ/3.1 protocol.
        if (it->type->sf_unsubscribe ((void*) (core_t*) this,
              it->instance, data + 1, size - 1) == 1)
#endif
            return dist.send_to_all (msg_, flags_);
        else
            return 0;
    }

    xs_assert (false);
    return -1;
}

bool xs::xsub_t::xhas_out ()
{
    //  Subscription can be added/removed anytime.
    return true;
}

int xs::xsub_t::xrecv (msg_t *msg_, int flags_)
{
    //  If there's already a message prepared by a previous call to xs_poll,
    //  return it straight ahead.
    if (has_message) {
        int rc = msg_->move (message);
        errno_assert (rc == 0);
        has_message = false;
        more = msg_->flags () & msg_t::more ? true : false;
        return 0;
    }

    //  TODO: This can result in infinite loop in the case of continuous
    //  stream of non-matching messages which breaks the non-blocking recv
    //  semantics.
    while (true) {

        //  Get a message using fair queueing algorithm.
        int rc = fq.recv (msg_, flags_);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0)
            return -1;

        //  Check whether the message matches at least one subscription.
        //  Non-initial parts of the message are passed 
        if (more || !options.filter || match (msg_)) {
            more = msg_->flags () & msg_t::more ? true : false;
            return 0;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (msg_->flags () & msg_t::more) {
            rc = fq.recv (msg_, XS_DONTWAIT);
            xs_assert (rc == 0);
        }
    }
}

bool xs::xsub_t::xhas_in ()
{
    //  There are subsequent parts of the partly-read message available.
    if (more)
        return true;

    //  If there's already a message prepared by a previous call to xs_poll,
    //  return straight ahead.
    if (has_message)
        return true;

    //  TODO: This can result in infinite loop in the case of continuous
    //  stream of non-matching messages.
    while (true) {

        //  Get a message using fair queueing algorithm.
        int rc = fq.recv (&message, XS_DONTWAIT);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0) {
            xs_assert (errno == EAGAIN);
            return false;
        }

        //  Check whether the message matches at least one subscription.
        if (!options.filter || match (&message)) {
            has_message = true;
            return true;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (message.flags () & msg_t::more) {
            rc = fq.recv (&message, XS_DONTWAIT);
            xs_assert (rc == 0);
        }
    }
}

bool xs::xsub_t::match (msg_t *msg_)
{
    for (filters_t::iterator it = filters.begin (); it != filters.end (); ++it)
        if (it->type->sf_match ((void*) (core_t*) this, it->instance,
              (unsigned char*) msg_->data (), msg_->size ()))
            return true;
    return false;
}

int xs::xsub_t::filter_subscribed (const unsigned char *data_, size_t size_)
{
    //  Create the subsctription message.
    msg_t msg;
    int rc = msg.init_size (size_ + 4);
    xs_assert (rc == 0);
    unsigned char *data = (unsigned char*) msg.data ();
    put_uint16 (data, XS_CMD_SUBSCRIBE);
    put_uint16 (data + 2, tmp_filter_id);
    memcpy (data + 4, data_, size_);

#if 0
    //  TODO: This is 0MQ/3.1 protocol.
    xs_assert (tmp_filter_id == XS_FILTER_PREFIX);
    msg_t msg;
    int rc = msg.init_size (size_ + 1);
    xs_assert (rc == 0);
    unsigned char *data = (unsigned char*) msg.data ();
    data [0] = 1;
    memcpy (data + 1, data_, size_);
#endif

    //  Send it to the pipe.
    bool sent = tmp_pipe->write (&msg);

    //  If we reached the SNDHWM, and thus cannot send the subscription, drop
    //  the subscription message instead. This matches the behaviour of
    //  xs_setsockopt(XS_SUBSCRIBE, ...), which also drops subscriptions
    //  when the SNDHWM is reached.
    if (!sent)
        msg.close ();

    return 0;
}

xs::xsub_session_t::xsub_session_t (io_thread_t *io_thread_, bool connect_,
      socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    session_base_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::xsub_session_t::~xsub_session_t ()
{
}

