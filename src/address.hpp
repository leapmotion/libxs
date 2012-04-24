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

#ifndef __XS_ADDRESS_HPP_INCLUDED__
#define __XS_ADDRESS_HPP_INCLUDED__

#include "platform.hpp"

#if defined XS_HAVE_WINDOWS
#include "windows.hpp"
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#endif

#if !defined XS_HAVE_WINDOWS && !defined XS_HAVE_OPENVMS
#include <sys/un.h>
#endif

namespace xs
{

     typedef struct sockaddr_storage address_t;

    //  This function translates textual TCP address into an address
    //  strcuture. If 'local' is true, names are resolved as local interface
    //  names. If it is false, names are resolved as remote hostnames.
    //  If 'ipv4only' is true, the name will never resolve to IPv6 address.
    int address_resolve_tcp (address_t *self_, const char *name_, bool local_,
        bool ipv4only_, bool ignore_port_=false);

    //  Resolves IPC (UNIX domain socket) address.
    int address_resolve_ipc (address_t *self_, const char *name_);

    //  Returns actual size of the address.
    socklen_t address_size (address_t *self_);
    
}

#endif

