/*
    Copyright (c) 2012 Martin Lucina <martin@lucina.net>

    This file is part of Crossroads I/O.

    Crossroads I/O is free software; you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    Crossroads I/O is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "testutil.hpp"

int XS_TEST_MAIN ()
{
    fprintf (stderr, "pubsub_udp test running...\n");

    void *ctx = xs_init ();
    assert (ctx);

    void *pub = xs_socket (ctx, XS_PUB);
    assert (pub);
    int rc = xs_bind (pub, "udp://127.0.0.1:5555");
    assert (rc != -1);

    void *sub = xs_socket (ctx, XS_SUB);
    assert (sub);
    rc = xs_connect (sub, "udp://127.0.0.1:5555");
    assert (rc != -1);
    rc = xs_setsockopt (sub, XS_SUBSCRIBE, "", 0);
    assert (rc == 0);

    //  Just in case there's an delay in lower parts of the network stack.
    sleep (1);
    
    const char *content = "12345678ABCDEFGH12345678abcdefgh";

    //  Send a message with two identical parts.
    rc = xs_send (pub, content, 32, XS_SNDMORE);
    assert (rc == 32);
    rc = xs_send (pub, content, 32, 0);
    assert (rc == 32);
    
    //  Receive the first part.
    char rcvbuf [32];
    int rcvmore = 0;
    size_t rcvmore_sz = sizeof rcvmore;
    rc = xs_recv (sub, rcvbuf, 32, 0);
    assert (rc == 32);
    rc = xs_getsockopt (sub, XS_RCVMORE, &rcvmore, &rcvmore_sz);
    assert (rc == 0);

    //  There must be one more part to receive.
    assert (rcvmore);
    //  And the content must match what was sent.
    assert (memcmp (rcvbuf, content, 32) == 0);

    //  Receive the second part.
    rc = xs_recv (sub, rcvbuf, 32, 0);
    assert (rc == 32);
    rcvmore_sz = sizeof rcvmore;
    rc = xs_getsockopt (sub, XS_RCVMORE, &rcvmore, &rcvmore_sz);
    assert (rc == 0);

    //  There must not be another part.
    assert (!rcvmore);
    //  And the content must match what was sent.
    assert (memcmp (rcvbuf, content, 32) == 0);

    rc = xs_close (pub);
    assert (rc == 0);

    rc = xs_close (sub);
    assert (rc == 0);

    rc = xs_term (ctx);
    assert (rc == 0);

    return 0 ;
}
