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

#include "err.hpp"
#include "platform.hpp"

const char *xs::errno_to_string (int errno_)
{
    switch (errno_) {
#if defined XS_HAVE_WINDOWS
    case ENOTSUP:
        return "Not supported";
    case EPROTONOSUPPORT:
        return "Protocol not supported";
    case ENOBUFS:
        return "No buffer space available";
    case ENETDOWN:
        return "Network is down";
    case EADDRINUSE:
        return "Address in use";
    case EADDRNOTAVAIL:
        return "Address not available";
    case ECONNREFUSED:
        return "Connection refused";
    case EINPROGRESS:
        return "Operation in progress";
#endif
    case EFSM:
        return "Operation cannot be accomplished in current state";
    case ENOCOMPATPROTO:
        return "The protocol is not compatible with the socket type";
    case ETERM:
        return "Context was terminated";
    default:
#if defined _MSC_VER
#pragma warning (push)
#pragma warning (disable:4996)
#endif
        return strerror (errno_);
#if defined _MSC_VER
#pragma warning (pop)
#endif
    }
}

void xs::xs_abort(const char *errmsg_)
{
#if defined XS_HAVE_WINDOWS

    //  Raise STATUS_FATAL_APP_EXIT.
    ULONG_PTR extra_info [1];
    extra_info [0] = (ULONG_PTR) errmsg_;
    RaiseException (0x40000015, EXCEPTION_NONCONTINUABLE, 1, extra_info);
#else
    abort ();
#endif
}

#ifdef XS_HAVE_WINDOWS

void xs::win_error (int err, char *buffer_, size_t buffer_size_)
{
    DWORD rc = FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL,
        SUBLANG_DEFAULT), buffer_, (DWORD) buffer_size_, NULL );
    xs_assert (rc);
}

void xs::wsa_error_to_errno ()
{
    int errcode = WSAGetLastError ();
    switch (errcode) {
    case WSAEINPROGRESS:
        errno = EAGAIN;
        return;
    case WSAEBADF:
        errno = EBADF;
        return;
    case WSAEINVAL:
        errno = EINVAL;
        return;
    case WSAEMFILE:
        errno = EMFILE;
        return;
    case WSAEFAULT:
        errno = EFAULT;
        return;
    case WSAEPROTONOSUPPORT:
        errno = EPROTONOSUPPORT;
        return;
    case WSAENOBUFS:
        errno = ENOBUFS;
        return;
    case WSAENETDOWN:
        errno = ENETDOWN;
        return;
    case WSAEADDRINUSE:
        errno = EADDRINUSE;
        return;
    case WSAEADDRNOTAVAIL:
        errno = EADDRNOTAVAIL;
        return;
    case WSAEAFNOSUPPORT:
        errno = EAFNOSUPPORT;
        return;
    case WSAEACCES:
        errno = EACCES;
        return;
    case WSAENETRESET:
        errno = ENETRESET;
        return;
    case WSAENETUNREACH:
        errno = ENETUNREACH;
        return;
    case WSAEHOSTUNREACH:
        errno = EHOSTUNREACH;
        return;
    case WSAENOTCONN:
        errno = ENOTCONN;
        return;
    case WSAEMSGSIZE:
        errno = EMSGSIZE;
        return;
    case WSAETIMEDOUT:
        errno = ETIMEDOUT;
        return;
    case WSAECONNREFUSED:
        errno = ECONNREFUSED;
        return;
    case WSAECONNABORTED:
        errno = ECONNABORTED;
        return;
    case WSAECONNRESET:
        errno = ECONNRESET;
        return;
    default:
        wsa_assert (false);
    }
}

#endif
