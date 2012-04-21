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

int XS_TEST_MAIN ()
{
    int rc;
    char buf [32];

    fprintf (stderr, "shutdown test running...\n");

    //  Create infrastructure.
    void *ctx = xs_init ();
    assert (ctx);
    void *push = xs_socket (ctx, XS_PUSH);
    assert (push);
    int push_id = xs_bind (push, "tcp://127.0.0.1:5560");
    assert (push_id != -1);
    void *pull = xs_socket (ctx, XS_PULL);
    assert (pull);
    rc = xs_connect (pull, "tcp://127.0.0.1:5560");
    assert (rc != -1);

    //  Pass one message through to ensure the connection is established.
    rc = xs_send (push, "ABC", 3, 0);
    assert (rc == 3);
    rc = xs_recv (pull, buf, sizeof (buf), 0);
    assert (rc == 3);

    //  Shut down the bound endpoint.
    rc = xs_shutdown (push, push_id);
    assert (rc == 0); 
    sleep (1);

    //  Check that sending would block (there's no outbound connection).
    rc = xs_send (push, "ABC", 3, XS_DONTWAIT);
    assert (rc == -1 && xs_errno () == EAGAIN);

    //  Clean up.
    rc = xs_close (pull);
    assert (rc == 0);
    rc = xs_close (push);
    assert (rc == 0);
    rc = xs_term (ctx);
    assert (rc == 0);

    //  Now the other way round.

    //  Create infrastructure.
    ctx = xs_init ();
    assert (ctx);
    pull = xs_socket (ctx, XS_PULL);
    assert (pull);
    rc = xs_bind (pull, "tcp://127.0.0.1:5560");
    assert (rc != -1);
    push = xs_socket (ctx, XS_PUSH);
    assert (push);
    push_id = xs_connect (push, "tcp://127.0.0.1:5560");
    assert (push_id != -1);

    //  Pass one message through to ensure the connection is established.
    rc = xs_send (push, "ABC", 3, 0);
    assert (rc == 3);
    rc = xs_recv (pull, buf, sizeof (buf), 0);
    assert (rc == 3);

    //  Shut down the bound endpoint.
    rc = xs_shutdown (push, push_id);
    assert (rc == 0); 
    sleep (1);

    //  Check that sending would block (there's no outbound connection).
    rc = xs_send (push, "ABC", 3, XS_DONTWAIT);
    assert (rc == -1 && xs_errno () == EAGAIN);

    //  Clean up.
    rc = xs_close (pull);
    assert (rc == 0);
    rc = xs_close (push);
    assert (rc == 0);
    rc = xs_term (ctx);
    assert (rc == 0);

    return 0;
}
