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

#include <algorithm>

#include "surveyor.hpp"
#include "err.hpp"
#include "msg.hpp"
#include "wire.hpp"
#include "likely.hpp"
#include "random.hpp"

xs::surveyor_t::surveyor_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xsurveyor_t (parent_, tid_, sid_),
    receiving_responses (false),
    survey_id (generate_random ()),
    timeout (0),
    has_prefetched (false)
{
    options.type = XS_SURVEYOR;

    prefetched.init ();
}

xs::surveyor_t::~surveyor_t ()
{
    prefetched.close ();
}

int xs::surveyor_t::xsend (msg_t *msg_, int flags_)
{
    int rc;

    //  Survey pattern works only with sigle-part messages.
    if (flags_ & XS_SNDMORE || msg_->flags () & msg_t::more) {
        errno = EINVAL;
        return -1;
    }

    //  Start the new survey. First, generate new survey ID.
    ++survey_id;
    msg_t id;
    rc = id.init_size (4);
    errno_assert (rc == 0);
    put_uint32 ((unsigned char*) id.data (), survey_id);
    id.set_flags (msg_t::more);
    rc = xsurveyor_t::xsend (&id, 0);
    if (rc != 0) {
        id.close ();
        return -1;
    }
    id.close ();

    //  Now send the body of the survey.
    rc = xsurveyor_t::xsend (msg_, flags_);
    errno_assert (rc == 0);

    //  Start waiting for responses from the peers.
    receiving_responses = true;

    //  Set up the timeout for the survey (-1 means infinite).
    if (!options.survey_timeout)
        timeout = -1;
    else
        timeout = now_ms () + options.survey_timeout;

    return 0;
}

int xs::surveyor_t::xrecv (msg_t *msg_, int flags_)
{
    int rc;

    //  If there's no survey underway, it's an error.
    if (!receiving_responses) {
        errno = EFSM;
        return -1;
    }

    //  If we have a message prefetched we can return it straight away.
    if (has_prefetched) {
        msg_->move (prefetched);
        has_prefetched = false;
        return 0;
    }

    //  Get the first part of the response -- the survey ID.
    rc = xsurveyor_t::xrecv (msg_, flags_);
    if (rc != 0) {
        if (errno != EAGAIN)
            return -1;

        //  In case of AGAIN we should check whether the survey timeout expired.
        //  If so, we should return ETIMEDOUT so that user is able to
        //  distinguish survey timeout from RCVTIMEO-caused timeout.
        errno = now_ms () >= timeout ? ETIMEDOUT : EAGAIN;
        return -1;
    }

    //  Check whether this is response for the onging survey. If not, we can
    //  drop the response.
    if (unlikely (!(msg_->flags () & msg_t::more) || msg_->size () != 4 ||
          get_uint32 ((unsigned char*) msg_->data ()) != survey_id)) {
        while (true) {
            rc = xsurveyor_t::xrecv (msg_, flags_);
            errno_assert (rc == 0);
            if (!(msg_->flags () & msg_t::more))
                break;
        }
        msg_->close ();
        msg_->init ();
        errno = EAGAIN;
        return -1;
    }

    //  Get the body of the response.
    rc = xsurveyor_t::xrecv (msg_, flags_);
    errno_assert (rc == 0);

    return 0;
}

bool xs::surveyor_t::xhas_in ()
{
    //  When no survey is underway, POLLIN is never signaled.
    if (!receiving_responses)
        return false;

    //  If there's already a message prefetches signal POLLIN.
    if (has_prefetched)
        return true;

    //  Try to prefetch a message.
    int rc = xrecv (&prefetched, XS_DONTWAIT);

    //  No message available.
    if (rc != 0 && errno == EAGAIN)
        return false;

    errno_assert (rc == 0);

    //  We have a message prefetched. We can signal POLLIN now.
    has_prefetched = true;
    return true;
}

bool xs::surveyor_t::xhas_out ()
{
    return xsurveyor_t::xhas_out ();
}

int xs::surveyor_t::rcvtimeo ()
{
    int t = (int) (timeout - now_ms ());
    if (t < 0)
        return options.rcvtimeo;
    if (options.rcvtimeo < 0)
        return t;
    return std::min (t, options.rcvtimeo); 
}

xs::surveyor_session_t::surveyor_session_t (io_thread_t *io_thread_,
      bool connect_, socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    xsurveyor_session_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::surveyor_session_t::~surveyor_session_t ()
{
}

