/*
    Copyright (c) 2010-2012 250bpm s.r.o.
    Copyright (c) 2007-2011 iMatix Corporation
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

#include "thread.hpp"
#include "err.hpp"
#include "platform.hpp"

#ifdef XS_HAVE_WINDOWS

extern "C"
{
    static unsigned int __stdcall thread_routine (void *arg_)
    {
        xs::thread_t *self = (xs::thread_t*) arg_;
        self->tfn (self->arg);
        return 0;
    }
}

void xs::thread_start (xs::thread_t *self_, thread_fn *tfn_, void *arg_)
{
    self_->tfn = tfn_;
    self_->arg =arg_;
    self_->handle = (HANDLE) _beginthreadex (NULL, 0,
        &::thread_routine, self_, 0 , NULL);
    win_assert (self_->handle != NULL);    
}

void xs::thread_stop (xs::thread_t *self_)
{
    DWORD rc = WaitForSingleObject (self_->handle, INFINITE);
    win_assert (rc != WAIT_FAILED);
    BOOL rc2 = CloseHandle (self_->handle);
    win_assert (rc2 != 0);
}

#else

#include <signal.h>

extern "C"
{
    static void *thread_routine (void *arg_)
    {
#if !defined XS_HAVE_OPENVMS && !defined XS_HAVE_ANDROID
        //  Following code will guarantee more predictable latencies as it'll
        //  disallow any signal handling in the I/O thread.
        sigset_t signal_set;
        int rc = sigfillset (&signal_set);
        errno_assert (rc == 0);
        rc = pthread_sigmask (SIG_BLOCK, &signal_set, NULL);
        posix_assert (rc);
#endif

        xs::thread_t *self = (xs::thread_t*) arg_;   
        self->tfn (self->arg);
        return NULL;
    }
}

void xs::thread_start (xs::thread_t *self_, thread_fn *tfn_, void *arg_)
{
    self_->tfn = tfn_;
    self_->arg =arg_;
    int rc = pthread_create (&self_->handle, NULL, thread_routine, self_);
    posix_assert (rc);
}

void xs::thread_stop (xs::thread_t *self_)
{
    int rc = pthread_join (self_->handle, NULL);
    posix_assert (rc);
}

#endif

