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
