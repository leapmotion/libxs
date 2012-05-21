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

#ifndef __XS_SUB_HPP_INCLUDED__
#define __XS_SUB_HPP_INCLUDED__

#include <vector>

#include "../include/xs/xs.h"

#include "xsub.hpp"
#include "core.hpp"

namespace xs
{

    class ctx_t;
    class msg_t;
    class io_thread_t;
    class socket_base_t;

    class sub_t : public xsub_t, public core_t
    {
    public:

        sub_t (xs::ctx_t *parent_, uint32_t tid_, int sid_);
        ~sub_t ();

    private:

        //  Overloaded functions from socket_base_t.
        int xsetsockopt (int option_, const void *optval_, size_t optvallen_);
        int xsend (xs::msg_t *msg_, int flags_);
        int xrecv (xs::msg_t *msg_, int flags_);
        bool xhas_in ();
        bool xhas_out ();

        //  Overloaded functions from core_t.
        int filter_subscribed (const unsigned char *data_, size_t size_);
        int filter_unsubscribed (const unsigned char *data_, size_t size_);

        //  The repository of subscriptions.
        struct filter_t
        {
            xs_filter_t *type;
            void *instance;
        };
        typedef std::vector <filter_t> filters_t;
        filters_t filters;

        //  If true, part of a multipart message was already received, but
        //  there are following parts still waiting.
        bool more;

        //  If true, 'message' contains a matching message to return on the
        //  next recv call.
        bool has_message;
        msg_t message;

        //  Check whether the message matches at least one subscription.
        bool match (xs::msg_t *msg_);

        sub_t (const sub_t&);
        const sub_t &operator = (const sub_t&);
    };

    class sub_session_t : public xsub_session_t
    {
    public:

        sub_session_t (xs::io_thread_t *io_thread_, bool connect_,
            xs::socket_base_t *socket_, const options_t &options_,
            const char *protocol_, const char *address_);
        ~sub_session_t ();

    private:

        sub_session_t (const sub_session_t&);
        const sub_session_t &operator = (const sub_session_t&);
    };

}

#endif
