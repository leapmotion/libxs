/*
    Copyright (c) 2009-2012 250bpm s.r.o.
    Copyright (c) 2007-2009 iMatix Corporation
    Copyright (c) 2011 VMware, Inc.
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

#include "req.hpp"
#include "err.hpp"
#include "msg.hpp"
#include "wire.hpp"
#include "random.hpp"
#include "likely.hpp"

xs::req_t::req_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xreq_t (parent_, tid_, sid_),
    receiving_reply (false),
    message_begins (true),
    request_id (generate_random ())
{
    options.type = XS_REQ;
}

xs::req_t::~req_t ()
{
}

int xs::req_t::xsend (msg_t *msg_, int flags_)
{
    int rc;

    //  If we've sent a request and we still haven't got the reply,
    //  we can't send another request.
    if (receiving_reply) {
        errno = EFSM;
        return -1;
    }

    //  Request starts with request ID and delimiter.
    if (message_begins) {
        if (options.sp_version >= 3) {
            msg_t id;
            rc = id.init_size (4);
            errno_assert (rc == 0);
            put_uint32 ((unsigned char*) id.data (), request_id);
            id.set_flags (msg_t::more);
            rc = xreq_t::xsend (&id, 0);
            if (rc != 0) {
                id.close ();
                return -1;
            }
            id.close ();
        }
        msg_t bottom;
        rc = bottom.init ();
        errno_assert (rc == 0);
        bottom.set_flags (msg_t::more);
        rc = xreq_t::xsend (&bottom, 0);
        if (rc != 0) {
            bottom.close ();
            return -1;
        }
        bottom.close ();
        message_begins = false;
    }

    bool more = msg_->flags () & msg_t::more ? true : false;

    rc = xreq_t::xsend (msg_, flags_);
    if (rc != 0)
        return rc;

    //  If the request was fully sent, flip the FSM into reply-receiving state.
    if (!more) {
        receiving_reply = true;
        message_begins = true;
    }

    return 0;
}

int xs::req_t::xrecv (msg_t *msg_, int flags_)
{
    int rc;

    //  If request wasn't send, we can't wait for reply.
    if (!receiving_reply) {
        errno = EFSM;
        return -1;
    }

    //  First part of the reply should be the original request ID,
    //  then delimiter.
    if (message_begins) {
retry:
        rc = xreq_t::xrecv (msg_, flags_);
        if (rc != 0)
            return rc;

        if (options.sp_version >= 3) {
            if (unlikely (!(msg_->flags () & msg_t::more) ||
                  msg_->size () != 4 ||
                  get_uint32 ((unsigned char*) msg_->data ()) != request_id)) {
                while (true) {
                    rc = xreq_t::xrecv (msg_, flags_);
                    errno_assert (rc == 0);
                    if (!(msg_->flags () & msg_t::more))
                        break;
                }
                msg_->close ();
                msg_->init ();
                goto retry;
            }
            ++request_id;
            rc = xreq_t::xrecv (msg_, flags_);
            errno_assert (rc == 0);
        }

        if (unlikely (!(msg_->flags () & msg_t::more) || msg_->size () != 0)) {
            while (true) {
                rc = xreq_t::xrecv (msg_, flags_);
                errno_assert (rc == 0);
                if (!(msg_->flags () & msg_t::more))
                    break;
            }
            msg_->close ();
            msg_->init ();
            goto retry;
        }

        message_begins = false;
    }

    rc = xreq_t::xrecv (msg_, flags_);
    if (rc != 0)
        return rc;

    //  If the reply is fully received, flip the FSM into request-sending state.
    if (!(msg_->flags () & msg_t::more)) {
        receiving_reply = false;
        message_begins = true;
    }

    return 0;
}

bool xs::req_t::xhas_in ()
{
    //  TODO: Duplicates should be removed here.

    if (!receiving_reply)
        return false;

    return xreq_t::xhas_in ();
}

bool xs::req_t::xhas_out ()
{
    if (receiving_reply)
        return false;

    return xreq_t::xhas_out ();
}

xs::req_session_t::req_session_t (io_thread_t *io_thread_, bool connect_,
      socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    xreq_session_t (io_thread_, connect_, socket_, options_, protocol_,
        address_),
    state (identity)
{
}

xs::req_session_t::~req_session_t ()
{
    state = options.recv_identity ? identity : bottom;
}

int xs::req_session_t::write (msg_t *msg_)
{
    switch (state) {
    case reqid:
        if (msg_->flags () == msg_t::more && msg_->size () == 4) {
            state = bottom;
            return xreq_session_t::write (msg_);
        }
        break;
    case bottom:
        if (msg_->flags () == msg_t::more && msg_->size () == 0) {
            state = body;
            return xreq_session_t::write (msg_);
        }
        break;
    case body:
        if (msg_->flags () == msg_t::more)
            return xreq_session_t::write (msg_);
        if (msg_->flags () == 0) {
            state = bottom;
            return xreq_session_t::write (msg_);
        }
        break;
    case identity:
        if (msg_->flags () == 0) {
            if (options.sp_version >= 3)
                state = reqid;
            else
                state = bottom;
            return xreq_session_t::write (msg_);
        }
        break;
    }
    errno = EFAULT;
    return -1;
}

void xs::req_session_t::detach ()
{
    state = options.recv_identity ? identity : bottom;
    xreq_session_t::detach ();
}

