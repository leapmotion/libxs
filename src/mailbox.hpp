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

#ifndef __XS_MAILBOX_HPP_INCLUDED__
#define __XS_MAILBOX_HPP_INCLUDED__

#include "signaler.hpp"
#include "config.hpp"
#include "command.hpp"
#include "ypipe.hpp"
#include "mutex.hpp"
#include "fd.hpp"

namespace xs
{

    //  Mailbox stores a list of commands sent to a particular object.
    //  Multiple threads can send commands to the mailbox in parallel.
    //  Only a single thread can read commands from the mailbox.

    typedef struct
    {
        //  The pipe to store actual commands.
        typedef ypipe_t <command_t, command_pipe_granularity> cpipe_t;
        cpipe_t cpipe;

        //  Signaler to pass signals from writer thread to reader thread.
        signaler_t signaler;

        //  There's only one thread receiving from the mailbox, but there
        //  is arbitrary number of threads sending. Given that ypipe requires
        //  synchronised access on both of its endpoints, we have to synchronise
        //  the sending side.
        mutex_t sync;

        //  True if the underlying pipe is active, ie. when we are allowed to
        //  read commands from it.
        bool active;

    }  mailbox_t;

    int mailbox_init (mailbox_t *self_);
    void mailbox_close (mailbox_t *self_);
    fd_t mailbox_fd (mailbox_t *self_);
    void mailbox_send (mailbox_t *self_, const command_t &cmd_);
    int mailbox_recv (mailbox_t *self_, command_t *cmd_, int timeout_);

}

#endif
