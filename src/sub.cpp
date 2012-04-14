/*
    Copyright (c) 2009-2012 250bpm s.r.o.
    Copyright (c) 2007-2009 iMatix Corporation
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

#include "sub.hpp"
#include "msg.hpp"
#include "wire.hpp"

xs::sub_t::sub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xsub_t (parent_, tid_, sid_),
    more (false),
    has_message (false)
{
    options.type = XS_SUB;

    int rc = message.init ();
    errno_assert (rc == 0);
}

xs::sub_t::~sub_t ()
{
    //  Deallocate all the filters.
    for (filters_t::iterator it = filters.begin (); it != filters.end (); ++it)
        it->type->sf_destroy ((void*) (core_t*) this, it->instance);

    int rc = message.close ();
    errno_assert (rc == 0);
}

int xs::sub_t::xsetsockopt (int option_, const void *optval_,
    size_t optvallen_)
{
    if (option_ != XS_SUBSCRIBE && option_ != XS_UNSUBSCRIBE) {
        errno = EINVAL;
        return -1;
    }

    if (optvallen_ > 0 && !optval_) {
        errno = EFAULT;
        return -1;
    }

    //  Find the relevant filter.
    filters_t::iterator it;
    for (it = filters.begin (); it != filters.end (); ++it)
        if (it->type->id (NULL) == options.filter)
            break;

    //  Process the subscription. If the filter of the specified type does not
    //  exist yet, create it.
    if (option_ == XS_SUBSCRIBE) {
        if (it == filters.end ()) {
            filter_t f;
            f.type = get_filter (options.filter);
            xs_assert (f.type);
            f.instance = f.type->sf_create ((void*) (core_t*) this);
            xs_assert (f.instance);
            filters.push_back (f);
            it = filters.end () - 1;
        }
        int rc = it->type->sf_subscribe ((void*) (core_t*) this, it->instance,
            (const unsigned char*) optval_, optvallen_);
        errno_assert (rc == 0);
        return 0;
    }
    else if (option_ == XS_UNSUBSCRIBE) {
        xs_assert (it != filters.end ());
        int rc = it->type->sf_unsubscribe ((void*) (core_t*) this, it->instance,
            (const unsigned char*) optval_, optvallen_);
        errno_assert (rc == 0);
        return 0;
    }

    xs_assert (false);
    return -1;
}

int xs::sub_t::xsend (msg_t *msg_, int flags_)
{
    //  Overload the XSUB's send.
    errno = ENOTSUP;
    return -1;
}

int xs::sub_t::xrecv (msg_t *msg_, int flags_)
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
        int rc = xsub_t::xrecv (msg_, flags_);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0)
            return -1;

        //  Check whether the message matches at least one subscription.
        //  Non-initial parts of the message are passed automatically.
        if (more || match (msg_)) {
            more = msg_->flags () & msg_t::more ? true : false;
            return 0;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (msg_->flags () & msg_t::more) {
            rc = xsub_t::xrecv (msg_, XS_DONTWAIT);
            xs_assert (rc == 0);
        }
    }
}

bool xs::sub_t::xhas_in ()
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
        int rc = xsub_t::xrecv (&message, XS_DONTWAIT);

        //  If there's no message available, return immediately.
        //  The same when error occurs.
        if (rc != 0) {
            xs_assert (errno == EAGAIN);
            return false;
        }

        //  Check whether the message matches at least one subscription.
        if (match (&message)) {
            has_message = true;
            return true;
        }

        //  Message doesn't match. Pop any remaining parts of the message
        //  from the pipe.
        while (message.flags () & msg_t::more) {
            rc = xsub_t::xrecv (&message, XS_DONTWAIT);
            xs_assert (rc == 0);
        }
    }
}

bool xs::sub_t::xhas_out ()
{
    //  Overload the XSUB's send.
    return false;
}

bool xs::sub_t::match (msg_t *msg_)
{
    for (filters_t::iterator it = filters.begin (); it != filters.end (); ++it)
        if (it->type->sf_match ((void*) (core_t*) this, it->instance,
              (unsigned char*) msg_->data (), msg_->size ()))
            return true;
    return false;
}

int xs::sub_t::filter_subscribed (const unsigned char *data_, size_t size_)
{
    //  Create the subscription message.
    msg_t msg;
    int rc = msg.init_size (size_ + 4);
    errno_assert (rc == 0);
    unsigned char *data = (unsigned char*) msg.data ();
    put_uint16 (data, XS_CMD_SUBSCRIBE);
    put_uint16 (data + 2, options.filter);
    memcpy (data + 4, data_, size_);

    //  Pass it further on in the stack.
    int err = 0;
    rc = xsub_t::xsend (&msg, 0);
    if (rc != 0)
        err = errno;
    int rc2 = msg.close ();
    errno_assert (rc2 == 0);
    if (rc != 0)
        errno = err;
    return rc;
}

int xs::sub_t::filter_unsubscribed (const unsigned char *data_, size_t size_)
{
    //  Create the unsubscription message.
    msg_t msg;
    int rc = msg.init_size (size_ + 4);
    errno_assert (rc == 0);
    unsigned char *data = (unsigned char*) msg.data ();
    put_uint16 (data, XS_CMD_UNSUBSCRIBE);
    put_uint16 (data + 2, options.filter);
    memcpy (data + 4, data_, size_);

    //  Pass it further on in the stack.
    int err = 0;
    rc = xsub_t::xsend (&msg, 0);
    if (rc != 0)
        err = errno;
    int rc2 = msg.close ();
    errno_assert (rc2 == 0);
    if (rc != 0)
        errno = err;
    return rc;
}

xs::sub_session_t::sub_session_t (io_thread_t *io_thread_, bool connect_,
      socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    xsub_session_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::sub_session_t::~sub_session_t ()
{
}

