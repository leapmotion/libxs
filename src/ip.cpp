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

#include "ip.hpp"
#include "err.hpp"
#include "platform.hpp"

#if defined XS_HAVE_WINDOWS
#include "windows.hpp"
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#if defined XS_HAVE_OPENVMS
#include <ioctl.h>
#endif

xs::fd_t xs::open_socket (int domain_, int type_, int protocol_)
{
    //  Setting this option result in sane behaviour when exec() functions
    //  are used. Old sockets are closed and don't block TCP ports etc.
#if defined XS_HAVE_SOCK_CLOEXEC
    type_ |= SOCK_CLOEXEC;
#endif

    fd_t s = socket (domain_, type_, protocol_);
    if (s == retired_fd)
        return retired_fd;

    //  If there's no SOCK_CLOEXEC, let's try the second best option. Note that
    //  race condition can cause socket not to be closed (if fork happens
    //  between socket creation and this point).
#if !defined XS_HAVE_SOCK_CLOEXEC && defined FD_CLOEXEC
    int rc = fcntl (s, F_SETFD, FD_CLOEXEC);
    errno_assert (rc != -1);
#endif

    //  On Windows, preventing sockets to be inherited by child processes is
    //  done using SetHandleInformation function.
#if defined XS_HAVE_WINDOWS && defined HANDLE_FLAG_INHERIT
    BOOL brc = SetHandleInformation ((HANDLE) s, HANDLE_FLAG_INHERIT, 0);
    win_assert (brc);
#endif

    return s;
}

void xs::tune_tcp_socket (fd_t s_, bool keepalive_)
{
    //  Disable Nagle's algorithm. We are doing data batching on Crossroads
    //  level, so using Nagle wouldn't improve throughput in anyway, but it
    //  would hurt latency.
    int nodelay = 1;
    int rc = setsockopt (s_, IPPROTO_TCP, TCP_NODELAY, (char*) &nodelay,
        sizeof (int));
#ifdef XS_HAVE_WINDOWS
    wsa_assert (rc != SOCKET_ERROR);
#else
    errno_assert (rc == 0);
#endif

#ifdef XS_HAVE_OPENVMS
    //  Disable delayed acknowledgements as they hurt latency is serious manner.
    int nodelack = 1;
    rc = setsockopt (s_, IPPROTO_TCP, TCP_NODELACK, (char*) &nodelack,
        sizeof (int));
    errno_assert (rc != SOCKET_ERROR);
#endif

    if (keepalive_) { 
        int keepalive = 1;
        rc = setsockopt (s_, SOL_SOCKET, SO_KEEPALIVE, (char*) &keepalive,
            sizeof (int));
#ifdef XS_HAVE_WINDOWS
        wsa_assert (rc != SOCKET_ERROR);
#else
        errno_assert (rc == 0);
#endif
    }
}

void xs::unblock_socket (fd_t s_)
{
#ifdef XS_HAVE_WINDOWS
    u_long nonblock = 1;
    int rc = ioctlsocket (s_, FIONBIO, &nonblock);
    wsa_assert (rc != SOCKET_ERROR);
#elif XS_HAVE_OPENVMS
	int nonblock = 1;
	int rc = ioctl (s_, FIONBIO, &nonblock);
    errno_assert (rc != -1);
#else
	int flags = fcntl (s_, F_GETFL, 0);
	if (flags == -1)
        flags = 0;
	int rc = fcntl (s_, F_SETFL, flags | O_NONBLOCK);
    errno_assert (rc != -1);
#endif
}

void xs::enable_ipv4_mapping (fd_t s_)
{
#ifdef IPV6_V6ONLY
#ifdef XS_HAVE_WINDOWS
    DWORD flag = 0;
#else
    int flag = 0;
#endif
    int rc = setsockopt (s_, IPPROTO_IPV6, IPV6_V6ONLY, (const char*) &flag,
        sizeof (flag));
#ifdef XS_HAVE_WINDOWS
    wsa_assert (rc != SOCKET_ERROR);
#else
    errno_assert (rc == 0);
#endif
#endif
}

