/*
    Copyright (c) 2010-2012 250bpm s.r.o.
    Copyright (c) 2010-2011 Other contributors as noted in the AUTHORS file

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

#ifndef __XS_SIGNALER_HPP_INCLUDED__
#define __XS_SIGNALER_HPP_INCLUDED__

#include "fd.hpp"
#include "polling.hpp"

//  On AIX, poll.h has to be included before xs.h to get consistent
//  definition of pollfd structure (AIX uses 'reqevents' and 'retnevents'
//  instead of 'events' and 'revents' and defines macros to map from POSIX-y
//  names to AIX-specific names).
#if defined XS_USE_SYNC_POLL
#   if HAVE_SYS_TYPES
#       include <sys/types.h>
#   endif
#   if HAVE_SYS_SELECT_H
#       include <sys/select.h>
#   endif
#   if HAVE_POLL_H
#       include <poll.h>
#   elif HAVE_SYS_POLL_H
#       include <sys/poll.h>
#   endif
#elif defined XS_USE_SYNC_SELECT
#   if defined XS_HAVE_WINDOWS
#       include "windows.hpp"
#   else
#       if HAVE_SYS_TYPES_H
#           include <sys/types.h>
#       endif
#       if HAVE_SYS_TIME_H
#           include <sys/time.h>
#       endif
#       if HAVE_TIME_H
#           include <time.h>
#       endif
#       if HAVE_UNISTD_H
#           include <unistd.h>
#       endif
#       if HAVE_SYS_SELECT_H
#           include <sys/select.h>
#       endif
#   endif
#endif

namespace xs
{

    //  This is a cross-platform equivalent to signal_fd. However, as opposed
    //  to signal_fd there can be at most one signal in the signaler at any
    //  given moment. Attempt to send a signal before receiving the previous
    //  one will result in undefined behaviour.

    typedef struct {
        fd_t w;
        fd_t r;
#if defined XS_USE_SYNC_SELECT
        fd_set fds;
#endif
    } signaler_t;

    //  Initialise the signaler.
    int signaler_init (signaler_t *self_);

    //  Destroy the signaler.
    void signaler_close (signaler_t *self_);

    //  Return file decriptor that you can poll on to get notified when
    //  signal is sent.
    fd_t signaler_fd (signaler_t *self_);

    //  Send a signal.
    void signaler_send (signaler_t *self_);

    //  Wait for a signal. up to timout_ milliseconds.
    //  The signale is *not* consumed by this function.
    int signaler_wait (signaler_t *self_, int timeout_);

    //  Wait for and consume a signal.
    void signaler_recv (signaler_t *self_);

}

#endif
