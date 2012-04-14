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

#include "mailbox.hpp"
#include "err.hpp"

int xs::mailbox_init (mailbox_t *self_)
{
    //  Initlialise the signaler.
    int rc = signaler_init (&self_->signaler);
    if (rc != 0)
        return -1;

    //  Get the pipe into passive state. That way, if the users starts by
    //  polling on the associated file descriptor it will get woken up when
    //  new command is posted.
    bool ok = self_->cpipe.read (NULL);
    xs_assert (!ok);
    self_->active = false;
    return 0;
}

void xs::mailbox_close (mailbox_t *self_)
{
    //  Deallocate the signaler.
    signaler_close (&self_->signaler);

    //  TODO: Retrieve and deallocate commands inside the cpipe.
}

xs::fd_t xs::mailbox_fd (mailbox_t *self_)
{
    return signaler_fd (&self_->signaler);
}

void xs::mailbox_send (mailbox_t *self_, const command_t &cmd_)
{
    self_->sync.lock ();
    self_->cpipe.write (cmd_, false);
    bool ok = self_->cpipe.flush ();
    self_->sync.unlock ();
    if (!ok)
        signaler_send (&self_->signaler);
}

int xs::mailbox_recv (mailbox_t *self_, command_t *cmd_, int timeout_)
{
    //  Try to get the command straight away.
    if (self_->active) {
        bool ok = self_->cpipe.read (cmd_);
        if (ok)
            return 0;

        //  If there are no more commands available, switch into passive state.
        self_->active = false;
        signaler_recv (&self_->signaler);
    }

    //  Wait for signal from the command sender.
    int rc = signaler_wait (&self_->signaler, timeout_);
    if (rc != 0 && (errno == EAGAIN || errno == EINTR))
        return -1;
    errno_assert (rc == 0);

    //  We've got the signal. Now we can switch into active state.
    self_->active = true;

    //  Get a command.
    bool ok = self_->cpipe.read (cmd_);
    xs_assert (ok);
    return 0;
}

