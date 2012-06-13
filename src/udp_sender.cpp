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

#include "platform.hpp"

#include <stdlib.h>

#include "io_thread.hpp"
#include "udp_sender.hpp"
#include "session_base.hpp"
#include "err.hpp"
#include "wire.hpp"
#include "stdint.hpp"
#include "ip.hpp"

xs::udp_sender_t::udp_sender_t (io_thread_t *parent_,
      const options_t &options_) :
    io_object_t (parent_),
    options (options_),
    encoder (0),
    seq_no (1),
    backoff_timer (NULL)
{
}

xs::udp_sender_t::~udp_sender_t ()
{
    if (backoff_timer) {
        rm_timer (backoff_timer);
    }
}

int xs::udp_sender_t::init (const char *address_)
{
    int rc;

    rc = address_resolve_tcp (&address, address_, false, options.ipv4only);
    if (rc != 0)
        return rc;

    //  Create a connected UDP socket for a single peer, and make
    //  it non-blocking.
    socket = open_socket (address.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == -1)
        return -1;
    rc = ::connect (socket, (const sockaddr *)&address,
        address_size (&address));
    if (rc != 0)
        return -1;
    unblock_socket (socket);

    return 0;
}

void xs::udp_sender_t::plug (io_thread_t *io_thread_, session_base_t *session_)
{
    session = session_;
    encoder.set_session (session);

    //  Start polling.
    socket_handle = add_fd (socket);
    set_pollout (socket_handle);

    //  UDP cannot pass subscriptions upstream; for now just fake subscribing
    //  to all messages.
    msg_t msg;
    msg.init_size (1);
    *(unsigned char*) msg.data () = 1;
    int rc = session_->write (&msg);
    errno_assert (rc == 0);
    session_->flush ();
}

void xs::udp_sender_t::unplug ()
{
    rm_fd (socket_handle);

    session = NULL;
}

void xs::udp_sender_t::terminate ()
{
    unplug ();
    delete this;
}

//  Called when our pipe is reactivated (has more data for us).
void xs::udp_sender_t::activate_out ()
{
    //  Reactivate polling.
    set_pollout (socket_handle);
    //  Try and send more data.
    out_event (retired_fd);
}

void xs::udp_sender_t::activate_in ()
{
    xs_assert (false);
}

//  Called when POLLERR is fired on the socket.
void xs::udp_sender_t::in_event (fd_t fd_)
{
    //  Get the actual error code from the socket.
    int err = 0;
    socklen_t len = sizeof (err);
    int rc = getsockopt (socket, SOL_SOCKET, SO_ERROR, (char *) &err, &len);
    errno_assert (rc == 0);

    //  If we got ECONNREFUSED, there is no one on the other end.
    //  Stop sending for XS_RECONNECT_IVL milliseconds.
    if (err == ECONNREFUSED) {
        backoff_timer = add_timer (options.reconnect_ivl);
        //  Polling will be re-started by timer_event ().
        reset_pollout (socket_handle);
        return;
    }

    //  Any other error is a bug, for now.
    errno = err;
    errno_assert (false);
}

//  Called when POLLOUT is fired on the socket.
void xs::udp_sender_t::out_event (fd_t fd_)
{
    unsigned char *data_p = data;
    size_t size = pgm_max_tpdu - udp_header_size;
    int offset = 0;

    //  Get one packet of data from the encoder.
    encoder.get_data (&data_p, &size, &offset);
    //  If there is no data in the pipe
    if (size == 0) {
        //  Stop polling, we will be re-started by activate_out ().
        reset_pollout (socket_handle);
        return;
    }
    assert ((size + udp_header_size) <= pgm_max_tpdu);

    //  Prepare UDP header (seqno, offset).
    unsigned char udp_header [udp_header_size];
    put_uint32 (udp_header, seq_no);
    put_uint16 (udp_header + 4, (uint16_t) offset);

    //  Send out the message, ensuring the whole message was sent.
    struct iovec iov[2] = {
        { udp_header, udp_header_size },
        { data_p, size }
    };
    ssize_t write_bytes = writev (socket, iov, 2);

    //  If we got ECONNREFUSED, there is no one on the other end.
    //  Drop the packet and back off for XS_RECONNECT_IVL milliseconds.
    if (write_bytes == -1 && errno == ECONNREFUSED) {
        backoff_timer = add_timer (options.reconnect_ivl);
        //  Polling will be re-started by timer_event ().
        reset_pollout (socket_handle);
        //  Increase sequence number to ensure other end is notified of
        //  dropped packet.
        ++seq_no;
        return;
    }
    errno_assert (write_bytes > 0);
    assert ((size_t)write_bytes == (udp_header_size + size));

    //  Packet was sent successfully, increase sequence number.
    ++seq_no;
}

//  Called when our backoff timer expires.
void xs::udp_sender_t::timer_event (handle_t handle_)
{
    xs_assert (handle_ == backoff_timer);
    backoff_timer = NULL;

    //  Re-start polling.
    set_pollout (socket_handle);

    out_event (retired_fd);
}
