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

#ifndef __XS_SURVEYOR_HPP_INCLUDED__
#define __XS_SURVEYOR_HPP_INCLUDED__

#include "xsurveyor.hpp"
#include "stdint.hpp"
#include "msg.hpp"

namespace xs
{

    class ctx_t;
    class io_thread_t;
    class socket_base_t;

    class surveyor_t : public xsurveyor_t
    {
    public:

        surveyor_t (xs::ctx_t *parent_, uint32_t tid_, int sid_);
        ~surveyor_t ();

        //  Overloads of functions from socket_base_t.
        int xsend (xs::msg_t *msg_, int flags_);
        int xrecv (xs::msg_t *msg_, int flags_);
        bool xhas_in ();
        bool xhas_out ();
        int rcvtimeo ();

    private:

        //  If true, survey was already lauched and have no expriered yet.
        bool receiving_responses;

        //  The ID of the ongoing survey.
        uint32_t survey_id;

        //  The time instant when the current survey expires.
        uint64_t timeout;

        //  Inbound message prefetched during polling.
        bool has_prefetched;
        msg_t prefetched;

        surveyor_t (const surveyor_t&);
        const surveyor_t &operator = (const surveyor_t&);
    };

    class surveyor_session_t : public xsurveyor_session_t
    {
    public:

        surveyor_session_t (xs::io_thread_t *io_thread_, bool connect_,
            xs::socket_base_t *socket_, const options_t &options_,
            const char *protocol_, const char *address_);
        ~surveyor_session_t ();

    private:

        surveyor_session_t (const surveyor_session_t&);
        const surveyor_session_t &operator = (const surveyor_session_t&);
    };

}

#endif
