/*
    Copyright (c) 2012 250bpm s.r.o.
    Copyright (c) 2012 Other contributors as noted in the AUTHORS file

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

#include "respondent.hpp"
#include "err.hpp"
#include "msg.hpp"

xs::respondent_t::respondent_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xrespondent_t (parent_, tid_, sid_),
    sending_reply (false)
{
    options.type = XS_RESPONDENT;
}

xs::respondent_t::~respondent_t ()
{
}

int xs::respondent_t::xsend (msg_t *msg_, int flags_)
{
    //  If there's no ongoing survey, we cannot send reply.
    if (!sending_reply) {
        errno = EFSM;
        return -1;
    }

    //  Survey pattern doesn't support multipart messages.
    if (msg_->flags () & msg_t::more || flags_ & XS_SNDMORE) {
        errno = EINVAL;
        return -1;
    }

    //  Push message to the reply pipe.
    int rc = xrespondent_t::xsend (msg_, flags_);
    if (rc != 0)
        return rc;

    //  Flip the FSM back to request receiving state.
    sending_reply = false;

    return 0;
}

int xs::respondent_t::xrecv (msg_t *msg_, int flags_)
{
    //  If we are in middle of sending a reply, we cannot receive next survey.
    if (sending_reply) {
        errno = EFSM;
        return -1;
    }

    //  First thing to do when receiving a srvey is to copy all the labels
    //  to the reply pipe.
    while (true) {
        int rc = xrespondent_t::xrecv (msg_, flags_);
        if (rc != 0)
            return rc;
        if (!(msg_->flags () & msg_t::more))
            break;
        rc = xrespondent_t::xsend (msg_, flags_);
        errno_assert (rc == 0);
    }

    //  When whole survey is read, flip the FSM to reply-sending state.
    sending_reply = true;

    return 0;
}

bool xs::respondent_t::xhas_in ()
{
    if (sending_reply)
        return false;

    return xrespondent_t::xhas_in ();
}

bool xs::respondent_t::xhas_out ()
{
    if (!sending_reply)
        return false;

    return xrespondent_t::xhas_out ();
}

xs::respondent_session_t::respondent_session_t (io_thread_t *io_thread_,
      bool connect_, socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    xrespondent_session_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::respondent_session_t::~respondent_session_t ()
{
}

