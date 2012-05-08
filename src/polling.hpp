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

#ifndef __XS_POLLING_HPP_INCLUDED__
#define __XS_POLLING_HPP_INCLUDED__

#include "platform.hpp"

//  This header file contains code that decides what polling mechanisms
//  should be used in individual cases. It also includes appropriate headers.
//
//  ASYNC polling mechanism to drive event loops in the I/O threads.
//  SYNC polling mechanism is used in xs_poll() and for blocking API functions.

#if defined XS_FORCE_SELECT
#define XS_USE_ASYNC_SELECT
#elif defined XS_FORCE_POLL
#define XS_USE_ASYNC_POLL
#elif defined XS_FORCE_EPOLL
#define XS_USE_ASYNC_EPOLL
#elif defined XS_FORCE_DEVPOLL
#define XS_USE_ASYNC_DEVPOLL
#elif defined XS_FORCE_KQUEUE
#define XS_USE_ASYNC_KQUEUE
#elif defined XS_HAVE_EPOLL
#define XS_USE_ASYNC_EPOLL
#elif defined XS_HAVE_DEVPOLL
#define XS_USE_ASYNC_DEVPOLL
#elif defined XS_HAVE_KQUEUE
#define XS_USE_ASYNC_KQUEUE
#elif defined XS_HAVE_POLL
#define XS_USE_ASYNC_POLL
#elif defined XS_HAVE_SELECT
#define XS_USE_ASYNC_SELECT
#else
#error No polling mechanism available!
#endif

#if defined XS_FORCE_SELECT
#define XS_USE_SYNC_SELECT
#elif defined XS_FORCE_POLL
#define XS_USE_SYNC_POLL
#elif defined XS_HAVE_POLL
#define XS_USE_SYNC_POLL
#elif defined XS_HAVE_SELECT
#define XS_USE_SYNC_SELECT
#else
#error No polling mechanism available!
#endif

//  Conditionally include header files that might be required to use the poll()
//  or select() functions

#if defined XS_HAVE_WINDOWS // Windows-specific header files
#   include "windows.h"
#   include "winsock2.h"
#else // Header files for Unix-like operating systems
#   if HAVE_SYS_TYPES
#       include <sys/types.h>
#   endif
#   if HAVE_SYS_TIME_H
#       include <sys/time.h>
#   endif
#   if HAVE_TIME_H
#       include <time.h>
#   endif
#   if HAVE_UNISTD_H
#       include <unistd.h>
#   endif
#   if HAVE_SYS_SELECT_H
#       include <sys/select.h>
#   endif
#   if HAVE_POLL_H
#       include <poll.h>
#   elif HAVE_SYS_POLL_H
#       include <sys/poll.h>
#   endif
#endif

#endif // __XS_POLLING_HPP_INCLUDED__
