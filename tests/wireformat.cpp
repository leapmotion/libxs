/*
    Copyright (c) 2012 250bpm s.r.o.
    Copyright (c) 2012 Paul Colomiets
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

#if defined XS_HAVE_WINDOWS
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

int XS_TEST_MAIN ()
{
    fprintf (stderr, "wire format test running...\n");

#if defined XS_HAVE_WINDOWS
    WSADATA info;
    int wsarc = WSAStartup (MAKEWORD(1,1), &info);
    assert (wsarc == 0);
#endif

    //  Create the basic infrastructure.
    void *ctx = xs_init ();
    assert (ctx);
    void *push = xs_socket (ctx, XS_PUSH);
    assert (push);
    void *pull = xs_socket (ctx, XS_PULL);
    assert (push);

    //  Bind the peer and get the message.
    int rc = xs_bind (pull, "tcp://127.0.0.1:5560");
    assert (rc != -1);
    rc = xs_bind (push, "tcp://127.0.0.1:5561");
    assert (rc != -1);

    // Connect to the peer using raw sockets.
    int rpush = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr ("127.0.0.1");
    address.sin_port = htons (5560);
    rc = connect(rpush, (struct sockaddr*) &address, sizeof (address));
    assert (rc == 0);

    int rpull = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr ("127.0.0.1");
    address.sin_port = htons (5561);
    rc = connect (rpull, (struct sockaddr*) &address, sizeof (address));
    assert (rc == 0);

    // Let's send some data and check if it arrived
    rc = send (rpush, "\x04\0abc", 5, 0);
    assert (rc == 5);
    unsigned char buf [3];
	unsigned char buf2 [3];
    rc = xs_recv (pull, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (!memcmp (buf, "abc", 3));

    // Let's push this data into another socket
    rc = xs_send (push, buf, sizeof (buf), 0);
    assert (rc == 3);
    rc = recv (rpull, (char*) buf2, sizeof (buf2), 0);
    assert (rc == 3);
    assert (!memcmp (buf2, "\x04\0abc", 3));

#if defined XS_HAVE_WINDOWS
    rc = closesocket (rpush);
    assert (rc != SOCKET_ERROR);
    rc = closesocket(rpull);
    assert (rc != SOCKET_ERROR);
    WSACleanup ();
#else
    rc = close (rpush);
    assert (rc == 0);
    rc = close (rpull);
    assert (rc == 0);
#endif
    rc = xs_close (pull);
    assert (rc == 0);
    rc = xs_close (push);
    assert (rc == 0);

    rc = xs_term (ctx);
    assert (rc == 0);

    return 0 ;
}
