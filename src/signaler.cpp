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

#include "signaler.hpp"
#include "platform.hpp"
#include "likely.hpp"
#include "stdint.hpp"
#include "config.hpp"
#include "err.hpp"
#include "fd.hpp"
#include "ip.hpp"

#if defined XS_HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

#if defined XS_HAVE_WINDOWS
#include "windows.hpp"
#else
#include <unistd.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

#if defined XS_HAVE_OPENVMS
#include <ioctl.h>
#endif

static int make_fdpair (xs::fd_t *r_, xs::fd_t *w_)
{
#if defined XS_HAVE_EVENTFD

    // Create eventfd object.
#if defined EFD_CLOEXEC
    xs::fd_t fd = eventfd (0, EFD_CLOEXEC);
    if (fd == -1)
        return -1;
#else
    xs::fd_t fd = eventfd (0, 0);
    if (fd == -1)
        return -1;
#if defined FD_CLOEXEC
    int rc = fcntl (fd, F_SETFD, FD_CLOEXEC);
    errno_assert (rc != -1);
#endif
#endif
    *w_ = fd;
    *r_ = fd;
    return 0;

#elif defined XS_HAVE_WINDOWS

    //  On Windows we are using TCP sockets for in-process communication.
    //  That is a security hole -- other processes on the same box may connect
    //  to the bound TCP port and hook into internal signal processing of
    //  the library. To solve this problem we should use a proper in-process
    //  signaling mechanism such as private semaphore. However, on Windows,
    //  these cannot be polled on using select(). Other functions that allow
    //  polling on these objects (e.g. WaitForMulitpleObjects) don't allow
    //  to poll on sockets. Thus, the only way to fix the problem is to
    //  implement IOCP polling mechanism that allows to poll on both sockets
    //  and in-process synchronisation objects.

    //  Make the following critical section accessible to everyone.
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof (sa);
    sa.bInheritHandle = FALSE;
    SECURITY_DESCRIPTOR sd;
    BOOL ok = InitializeSecurityDescriptor (&sd, SECURITY_DESCRIPTOR_REVISION);
    win_assert (ok);
    ok = SetSecurityDescriptorDacl(&sd, TRUE, (PACL) NULL, FALSE);
    win_assert (ok);
    sa.lpSecurityDescriptor = &sd;

    //  This function has to be in a system-wide critical section so that
    //  two instances of the library don't accidentally create signaler
    //  crossing the process boundary.
    HANDLE sync = CreateEvent (&sa, FALSE, TRUE,
        "Global\\xs-signaler-port-sync");
    win_assert (sync != NULL);

    //  Enter the critical section.
    DWORD dwrc = WaitForSingleObject (sync, INFINITE);
    xs_assert (dwrc == WAIT_OBJECT_0);

    //  Windows has no 'socketpair' function. CreatePipe is no good as pipe
    //  handles cannot be polled on. Here we create the socketpair by hand.
    *w_ = INVALID_SOCKET;
    *r_ = INVALID_SOCKET;

    //  Create listening socket.
    SOCKET listener;
    listener = xs::open_socket (AF_INET, SOCK_STREAM, 0);
    if (listener == xs::retired_fd)
        return -1;

    //  Set SO_REUSEADDR and TCP_NODELAY on listening socket.
    BOOL so_reuseaddr = 1;
    int rc = setsockopt (listener, SOL_SOCKET, SO_REUSEADDR,
        (char *)&so_reuseaddr, sizeof (so_reuseaddr));
    wsa_assert (rc != SOCKET_ERROR);
    BOOL tcp_nodelay = 1;
    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY,
        (char *)&tcp_nodelay, sizeof (tcp_nodelay));
    wsa_assert (rc != SOCKET_ERROR);

    //  Bind listening socket to the local port.
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    addr.sin_port = htons (xs::signaler_port);
    rc = bind (listener, (const struct sockaddr*) &addr, sizeof (addr));
    wsa_assert (rc != SOCKET_ERROR);

    //  Listen for incomming connections.
    rc = listen (listener, 1);
    wsa_assert (rc != SOCKET_ERROR);

    //  Create the writer socket.
    *w_ = xs::open_socket (AF_INET, SOCK_STREAM, 0);
    if (*w_ == xs::retired_fd) {
        rc = closesocket (listener);
        wsa_assert (rc != SOCKET_ERROR);
        return -1;
    }

    //  Set TCP_NODELAY on writer socket.
    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELAY,
        (char *)&tcp_nodelay, sizeof (tcp_nodelay));
    wsa_assert (rc != SOCKET_ERROR);

    //  Connect writer to the listener.
    rc = connect (*w_, (sockaddr *) &addr, sizeof (addr));
    wsa_assert (rc != SOCKET_ERROR);

    //  Accept connection from writer.
    *r_ = accept (listener, NULL, NULL);
    if (*r_ == xs::retired_fd) {
        rc = closesocket (listener);
        wsa_assert (rc != SOCKET_ERROR);
        rc = closesocket (*w_);
        wsa_assert (rc != SOCKET_ERROR);
        return -1;
    }

    //  We don't need the listening socket anymore. Close it.
    rc = closesocket (listener);
    wsa_assert (rc != SOCKET_ERROR);

    //  Exit the critical section.
    BOOL brc = SetEvent (sync);
    win_assert (brc != 0);

    return 0;

#elif defined XS_HAVE_OPENVMS

    //  Whilst OpenVMS supports socketpair - it maps to AF_INET only.  Further,
    //  it does not set the socket options TCP_NODELAY and TCP_NODELACK which
    //  can lead to performance problems.
    //
    //  The bug will be fixed in V5.6 ECO4 and beyond.  In the meantime, we'll
    //  create the socket pair manually.
    sockaddr_in lcladdr;
    memset (&lcladdr, 0, sizeof (lcladdr));
    lcladdr.sin_family = AF_INET;
    lcladdr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    lcladdr.sin_port = 0;

    int listener = open_socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (listener != -1);

    int on = 1;
    int rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = bind(listener, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    socklen_t lcladdr_len = sizeof (lcladdr);

    rc = getsockname (listener, (struct sockaddr*) &lcladdr, &lcladdr_len);
    errno_assert (rc != -1);

    rc = listen (listener, 1);
    errno_assert (rc != -1);

    *w_ = open_socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (*w_ != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = connect (*w_, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    *r_ = accept (listener, NULL, NULL);
    errno_assert (*r_ != -1);

    close (listener);

    return 0;

#else // All other implementations support socketpair()

    int sv [2];
#if defined XS_HAVE_SOCK_CLOEXEC
    int rc = socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
    if (rc == -1)
        return -1;
#else
    int rc = socketpair (AF_UNIX, SOCK_STREAM, 0, sv);
    if (rc == -1)
        return -1;
    errno_assert (rc == 0);
#if defined FD_CLOEXEC
    rc = fcntl (sv [0], F_SETFD, FD_CLOEXEC);
    errno_assert (rc != -1);
    rc = fcntl (sv [1], F_SETFD, FD_CLOEXEC);
    errno_assert (rc != -1);
#endif
#endif
    *w_ = sv [0];
    *r_ = sv [1];
    return 0;

#endif
}

int xs::signaler_init (xs::signaler_t *self_)
{
    //  Create the socketpair for signaling.
    int rc = make_fdpair (&self_->r, &self_->w);
    if (rc != 0)
        return -1;

    //  Set both fds to non-blocking mode.
    unblock_socket (self_->w);
    unblock_socket (self_->r);

#if defined XS_USE_SYNC_SELECT
    FD_ZERO (&self_->fds);
#endif

    return 0;
}

void xs::signaler_close (xs::signaler_t *self_)
{
#if defined XS_HAVE_EVENTFD
    int rc = close (self_->r);
    errno_assert (rc == 0);
#elif defined XS_HAVE_WINDOWS
    int rc = closesocket (self_->w);
    wsa_assert (rc != SOCKET_ERROR);
    rc = closesocket (self_->r);
    wsa_assert (rc != SOCKET_ERROR);
#else
    int rc = close (self_->w);
    errno_assert (rc == 0);
    rc = close (self_->r);
    errno_assert (rc == 0);
#endif
}

xs::fd_t xs::signaler_fd (xs::signaler_t *self_)
{
    return self_->r;
}

void xs::signaler_send (xs::signaler_t *self_)
{
#if defined XS_HAVE_EVENTFD
    const uint64_t inc = 1;
    ssize_t sz = write (self_->w, &inc, sizeof (inc));
    errno_assert (sz == sizeof (inc));
#elif defined XS_HAVE_WINDOWS
    unsigned char dummy = 0;
    int nbytes = ::send (self_->w, (char*) &dummy, sizeof (dummy), 0);
    wsa_assert (nbytes != SOCKET_ERROR);
    xs_assert (nbytes == sizeof (dummy));
#else
    unsigned char dummy = 0;
    while (true) {
#if defined MSG_NOSIGNAL
        ssize_t nbytes = ::send (self_->w, &dummy, sizeof (dummy),
            MSG_NOSIGNAL);
#else
        ssize_t nbytes = ::send (self_->w, &dummy, sizeof (dummy), 0);
#endif
        if (unlikely (nbytes == -1 && errno == EINTR))
            continue;
        xs_assert (nbytes == sizeof (dummy));
        break;
    }
#endif
}

int xs::signaler_wait (xs::signaler_t *self_, int timeout_)
{
#ifdef XS_USE_SYNC_POLL

    struct pollfd pfd;
    pfd.fd = self_->r;
    pfd.events = POLLIN;
    int rc = poll (&pfd, 1, timeout_);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
    else if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
    xs_assert (rc == 1);
    xs_assert (pfd.revents & POLLIN);
    return 0;

#elif defined XS_USE_SYNC_SELECT
    FD_SET (self_->r, &self_->fds);
    struct timeval timeout;
    if (timeout_ >= 0) {
        timeout.tv_sec = timeout_ / 1000;
        timeout.tv_usec = timeout_ % 1000 * 1000;
    }
#ifdef XS_HAVE_WINDOWS
    int rc = select (0, &self_->fds, NULL, NULL,
        timeout_ >= 0 ? &timeout : NULL);
    wsa_assert (rc != SOCKET_ERROR);
#else
    int rc = select (self_->r + 1, &self_->fds, NULL, NULL,
        timeout_ >= 0 ? &timeout : NULL);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
#endif
    if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
    xs_assert (rc == 1);
    return 0;

#else
#error
    return -1;
#endif
}

void xs::signaler_recv (xs::signaler_t *self_)
{
    //  Attempt to read a signal.
#if defined XS_HAVE_EVENTFD
    uint64_t dummy;
    ssize_t sz = read (self_->r, &dummy, sizeof (dummy));
    errno_assert (sz == sizeof (dummy));

    //  If we accidentally grabbed the next signal along with the current
    //  one, return it back to the eventfd object.
    if (unlikely (dummy == 2)) {
        const uint64_t inc = 1;
        ssize_t sz = write (self_->w, &inc, sizeof (inc));
        errno_assert (sz == sizeof (inc));
        return;
    }

    xs_assert (dummy == 1);
#else
    unsigned char dummy;
#if defined XS_HAVE_WINDOWS
    int nbytes = ::recv (self_->r, (char*) &dummy, sizeof (dummy), 0);
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    ssize_t nbytes = ::recv (self_->r, &dummy, sizeof (dummy), 0);
    errno_assert (nbytes >= 0);
#endif
    xs_assert (nbytes == sizeof (dummy));
    xs_assert (dummy == 0);
#endif
}

