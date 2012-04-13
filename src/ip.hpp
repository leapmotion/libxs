/*
    Copyright (c) 2010-2012 250bpm s.r.o.
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

#ifndef __XS_IP_HPP_INCLUDED__
#define __XS_IP_HPP_INCLUDED__

#include "fd.hpp"

namespace xs
{

    //  Same as socket(2), but allows for transparent tweaking the options.
    //  These functions automatically tune the socket so there's no need to
    //  call tune_socket/tune_tcp_socket afterwards.
    fd_t open_socket (int domain_, int type_, int protocol_);
    fd_t open_tcp_socket (int domain_, bool keepalive_);

    //  Tunes the supplied socket. Use these functions if you've got
    //  the socket in some other way, not by open_socket/open_tcp_socket
    //  (e.g. using accept()).
    void tune_socket (fd_t s_);
    void tune_tcp_socket (fd_t s_, bool keeaplive_);

    //  Sets the socket into non-blocking mode.
    void unblock_socket (fd_t s_);

    //  Enable IPv4-mapping of addresses in case it is disabled by default.
    void enable_ipv4_mapping (fd_t s_);

}

#endif 
