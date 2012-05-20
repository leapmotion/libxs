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

#include <new>

#include "udp_receiver.hpp"
#include "session_base.hpp"
#include "stdint.hpp"
#include "wire.hpp"
#include "err.hpp"
#include "ip.hpp"

xs::udp_receiver_t::udp_receiver_t (class io_thread_t *parent_,
      const options_t &options_) :
    io_object_t (parent_),
    options (options_),
    pending_bytes (0),
    pending_p (NULL),
    session (NULL),
    last_seq_no (0)
{
    decoder = new (std::nothrow) decoder_t (in_batch_size, options.maxmsgsize);
    alloc_assert (decoder);
}

xs::udp_receiver_t::~udp_receiver_t ()
{
    delete decoder;
}

int xs::udp_receiver_t::init (const char *address_)
{
    int rc;

    rc = address_resolve_tcp (&address, address_, false, options.ipv4only);
    if (rc != 0)
        return rc;

    //  Create a unconnected UDP socket, bind it to the requested address
    //  and make it non-blocking.
    socket = open_socket (address.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == -1)
        return -1;
    rc = ::bind (socket, (const sockaddr *)&address, address_size (&address));
    if (rc != 0)
        return -1;
    unblock_socket (socket);

    return 0;
}

void xs::udp_receiver_t::plug (io_thread_t *io_thread_,
    session_base_t *session_)
{
    //  Start polling.
    socket_handle = add_fd (socket);
    set_pollin (socket_handle);

    session = session_;
    decoder->set_session (session);

    //  If there are any subscriptions already queued in the session, drop them.
    drop_subscriptions ();
}

void xs::udp_receiver_t::unplug ()
{
    rm_fd (socket_handle);

    session = NULL;
}

void xs::udp_receiver_t::terminate ()
{
    unplug ();
    delete this;
}

//  Called when our pipe is reactivated (has more data for us).
void xs::udp_receiver_t::activate_out ()
{
    drop_subscriptions ();
}

//  Called when our pipe is reactivated (able to accept more data).
void xs::udp_receiver_t::activate_in ()
{
    //  Process any pending data.
    if (pending_bytes > 0) {
        ssize_t processed_bytes = \
            decoder->process_buffer (pending_p, pending_bytes);
        //  Flush any messages produced by the decoder to the pipe.
        session->flush ();
        if (processed_bytes < pending_bytes) {
            //  Some data (still) could not be written to the pipe.
            pending_bytes -= processed_bytes;
            pending_p += processed_bytes;
            //  Try again later.
            return;
        }
        //  Done with unprocessed data.
        pending_bytes = 0;
    }

    //  Reactivate polling.
    set_pollin (socket_handle);

    //  Read any data that might have showed up on the socket in the mean time.
    in_event (retired_fd);
}

//  Called when POLLIN is fired on the socket.
void xs::udp_receiver_t::in_event (fd_t fd_)
{
    //  Receive a packet.
    ssize_t recv_bytes = recv (socket, data, sizeof data, 0);
    //  At the moment, go back to polling on EAGAIN and assert on any
    //  other error.
    if ((recv_bytes < 0) && errno == EAGAIN)
        return;
    assert (recv_bytes > 0);

    //  Parse UDP packet header.
    unsigned char *data_p = data;
    uint32_t seq_no = get_uint32 (data_p);
    uint16_t offset = get_uint16 (data_p + 4);
    data_p += udp_header_size;
    recv_bytes -= udp_header_size;

    //  If this is our first packet, join the message stream.
    if (last_seq_no == 0) {
        if (offset == 0xffff)
            return;
        else {
            data_p += offset;
            recv_bytes -= offset;
        }
    }
    //  Otherwise, decide based on the sequence number.
    else {
        //  If this packet is in sequence, process the whole packet.
        if ((last_seq_no + 1) == seq_no)
            ;
        //  Otherwise, if it is an old packet, drop it.
        else if (seq_no <= last_seq_no)
            return;
        //  Otherwise we have packet loss, rejoin the message stream.
        else {
            if (offset == 0xffff)
                return;
            else {
                data_p += offset;
                recv_bytes -= offset;

                //  Re-create decoder to clear state.
                delete decoder;
                decoder = NULL;
                decoder = new (std::nothrow) decoder_t (in_batch_size,
                    options.maxmsgsize);
                alloc_assert (decoder);
                decoder->set_session (session);
            }
        }
    }
    //  If we get here, we will process this packet and it becomes our
    //  last seen sequence number.
    last_seq_no = seq_no;

    //  Decode data and push it to our pipe.
    ssize_t processed_bytes = decoder->process_buffer (data_p, recv_bytes);
    if (processed_bytes < recv_bytes) {
        //  Some data could not be written to the pipe. Save it for later.
        pending_bytes = recv_bytes - processed_bytes;
        pending_p = data_p + processed_bytes;
        //  Stop polling. We will be restarted by a call to activate_in ().
        reset_pollin (socket_handle);
    }

    //  Flush any messages produced by the decoder to the pipe.
    session->flush ();
}

void xs::udp_receiver_t::out_event (fd_t fd_)
{
    assert (false);
}

void xs::udp_receiver_t::drop_subscriptions ()
{
    msg_t msg;
    while (session->read (&msg))
        msg.close ();
}
