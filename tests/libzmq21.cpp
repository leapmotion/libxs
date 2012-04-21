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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#if defined XS_HAVE_OPENVMS
#include <ioctl.h>
#endif

int XS_TEST_MAIN ()
{
    fprintf (stderr, "libzmq21 test running...\n");

#if defined XS_HAVE_WINDOWS
    WSADATA info;
    int wsarc = WSAStartup (MAKEWORD(1,1), &info);
    assert (wsarc == 0);
#endif

    //  First, test up-to-date publisher with 0MQ/2.1-style subscriber.

    //  Create the basic infrastructure.
    void *ctx = xs_init ();
    assert (ctx);
    void *pub = xs_socket (ctx, XS_PUB);
    assert (pub);
    int protocol = 1;
    int rc = xs_setsockopt (pub, XS_PROTOCOL, &protocol, sizeof (protocol));
    assert (rc == 0);
    rc = xs_bind (pub, "tcp://127.0.0.1:5560");
    assert (rc != -1);

    int oldsub = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr ("127.0.0.1");
    address.sin_port = htons (5560);
    rc = connect (oldsub, (struct sockaddr*) &address, sizeof (address));
    assert (rc == 0);

    //  Wait a while to allow connection to be accepted on the publisher side.
    sleep (1);

    //  Send a message and check whether it arrives although there was no
    //  subscription sent.
    rc = xs_send (pub, "ABC", 3, 0);
    assert (rc == 3);
    char buf [5];
    rc = recv (oldsub, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (!memcmp (buf, "\x04\0ABC", 5));

    //  Tear down the infrastructure.
    rc = xs_close (pub);
    assert (rc == 0);
#if defined XS_HAVE_WINDOWS
    rc = closesocket (oldsub);
    assert (rc != SOCKET_ERROR);
#else
    rc = close (oldsub);
    assert (rc == 0);
#endif
    rc = xs_term (ctx);
    assert (rc == 0);

    //  Second, test the 0MQ/2.1-style publisher with up-to-date subscriber.
    
    //  Create the basic infrastructure.
    ctx = xs_init ();
    assert (ctx);
    void *sub = xs_socket (ctx, XS_SUB);
    assert (sub);
    protocol = 1;
    rc = xs_setsockopt (sub, XS_PROTOCOL, &protocol, sizeof (protocol));
    assert (rc == 0);
    rc = xs_setsockopt (sub, XS_SUBSCRIBE, "", 0);
    assert (rc == 0);
    rc = xs_bind (sub, "tcp://127.0.0.1:5560");
    assert (rc != -1);

    int oldpub = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr ("127.0.0.1");
    address.sin_port = htons (5560);
    rc = connect (oldpub, (struct sockaddr*) &address, sizeof (address));
    assert (rc == 0);

    //  Wait a while to allow connection to be accepted on the subscriber side.
    sleep (1);

    //  Set the socket to the non-blocking mode.
    #ifdef XS_HAVE_WINDOWS
        u_long nonblock = 1;
        rc = ioctlsocket (oldpub, FIONBIO, &nonblock);
        assert (rc != SOCKET_ERROR);
    #elif XS_HAVE_OPENVMS
	    int nonblock = 1;
	    rc = ioctl (oldpub, FIONBIO, &nonblock);
        assert (rc != -1);
    #else
	    int flags = fcntl (oldpub, F_GETFL, 0);
	    if (flags == -1)
            flags = 0;
	    rc = fcntl (oldpub, F_SETFL, flags | O_NONBLOCK);
        assert (rc != -1);
    #endif

    //  Check that subscription haven't arrived at the publisher side.
    rc = recv (oldpub, buf, sizeof (buf), 0);
#if defined XS_HAVE_WINDOWS
    assert (rc == SOCKET_ERROR && WSAGetLastError () == WSAEWOULDBLOCK);
#else
    assert (rc == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
#endif

    //  Pass one message through.
    rc = send (oldpub, "\x04\0ABC", 5, 0);
    assert (rc == 5);
    rc = xs_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 3);

    //  Pass one message with usused flags set (0MQ/2.1 bug).
    rc = send (oldpub, "\x04\xfe" "ABC", 5, 0);
    assert (rc == 5);
    rc = xs_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 3);

    //  Tear down the infrastructure.
    rc = xs_close (sub);
    assert (rc == 0);
#if defined XS_HAVE_WINDOWS
    rc = closesocket (oldpub);
    assert (rc != SOCKET_ERROR);
#else
    rc = close (oldpub);
    assert (rc == 0);
#endif
    rc = xs_term (ctx);
    assert (rc == 0);

#if defined XS_HAVE_WINDOWS
    WSACleanup ();
#endif

    return 0 ;
}
