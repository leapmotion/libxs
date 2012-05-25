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

#include "testutil.hpp"

#define BACKLOG 10
#define PARALLEL_CONNECTS 50

#if 0

extern "C"
{

#if !defined XS_HAVE_WINDOWS && !defined XS_HAVE_OPENVMS
    static void ipc_worker (void *ctx_)
    {
        int rc;
        void *push;

        push = xs_socket (ctx_, XS_PUSH);
        assert (push);
        rc = xs_connect (push, "ipc:///tmp/test-backlog");
        assert (rc >= 0);
        rc = xs_send (push, NULL, 0, 0);
        assert (rc == 0);
        rc = xs_close (push);
        assert (rc == 0);
    }
#endif
    static void tcp_worker (void *ctx_)
    {
        int rc;
        void *push;

        push = xs_socket (ctx_, XS_PUSH);
        assert (push);
        rc = xs_connect (push, "tcp://127.0.0.1:5560");
        assert (rc >= 0);

        rc = xs_send (push, NULL, 0, 0);
        assert (rc == 0);
        rc = xs_close (push);
        assert (rc == 0);
    }
}
#endif

int XS_TEST_MAIN ()
{
    fprintf (stderr, "backlog test running...\n");

#if 0
    int rc;
    void *ctx;
    void *pull;
    int i;
    int backlog;
    void *threads [PARALLEL_CONNECTS];

    //  Test the exhaustion on IPC backlog.
#if !defined XS_HAVE_WINDOWS && !defined XS_HAVE_OPENVMS
    ctx = xs_init ();
    assert (ctx);

    pull = xs_socket (ctx, XS_PULL);
    assert (pull);
    backlog = BACKLOG;
    rc = xs_setsockopt (pull, XS_BACKLOG, &backlog, sizeof (backlog));
    assert (rc == 0);
    rc = xs_bind (pull, "ipc:///tmp/test-backlog");
    assert (rc >= 0);

    
    for (i = 0; i < PARALLEL_CONNECTS; i++)
        threads [i] = thread_create (ipc_worker, ctx);

    for (i = 0; i < PARALLEL_CONNECTS; i++) {
        rc = xs_recv (pull, NULL, 0, 0);
        assert (rc == 0);
    }
    
    rc = xs_close (pull);
    assert (rc == 0);
    rc = xs_term (ctx);
    assert (rc == 0);

    for (i = 0; i < PARALLEL_CONNECTS; i++)
        thread_join (threads [i]);
#endif

    //  Test the exhaustion on TCP backlog.

    ctx = xs_init ();
    assert (ctx);

    pull = xs_socket (ctx, XS_PULL);
    assert (pull);
    backlog = BACKLOG;
    rc = xs_setsockopt (pull, XS_BACKLOG, &backlog, sizeof (backlog));
    assert (rc == 0);
    rc = xs_bind (pull, "tcp://127.0.0.1:5560");
    assert (rc >= 0);

    for (i = 0; i < PARALLEL_CONNECTS; i++)
        threads [i] = thread_create (tcp_worker, ctx);

    for (i = 0; i < PARALLEL_CONNECTS; i++) {
        rc = xs_recv (pull, NULL, 0, 0);
        assert (rc == 0);
    }
    
    rc = xs_close (pull);
    assert (rc == 0);
    rc = xs_term (ctx);
    assert (rc == 0);

    for (i = 0; i < PARALLEL_CONNECTS; i++)
        thread_join (threads [i]);
#endif

    return 0;
}
