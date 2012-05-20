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

#ifndef __XS_UDP_SENDER_HPP_INCLUDED__
#define __XS_UDP_SENDER_HPP_INCLUDED__

#include "platform.hpp"

#include "stdint.hpp"
#include "io_object.hpp"
#include "i_engine.hpp"
#include "options.hpp"
#include "address.hpp"
#include "encoder.hpp"

namespace xs
{

    class io_thread_t;
    class session_base_t;

    class udp_sender_t : public io_object_t, public i_engine
    {

    public:

        udp_sender_t (xs::io_thread_t *parent_, const options_t &options_);
        ~udp_sender_t ();

        int init (const char *address_);

        //  i_engine interface implementation.
        void plug (xs::io_thread_t *io_thread_,
            xs::session_base_t *session_);
        void unplug ();
        void terminate ();
        void activate_in ();
        void activate_out ();

        //  i_poll_events interface implementation.
        void in_event (fd_t fd_);
        void out_event (fd_t fd_);
        void timer_event (handle_t handle_);

    private:

        //  Underlying UDP socket.
        fd_t socket;
        handle_t socket_handle;

        //  Socket address.
        address_t address;

        //  Socket options.
        options_t options;

        //  Encoder for this socket.
        encoder_t encoder;

        //  Associated session.
        xs::session_base_t *session;

        //  UDP packet header size.
        static const size_t udp_header_size = 6;

        //  Packet buffer for outgoing packets.
        unsigned char data [pgm_max_tpdu];

        //  Sequence number for outgoing packets.
        uint32_t seq_no;

        //  Backoff timer if receiver is not present.
        handle_t backoff_timer;

        udp_sender_t (const udp_sender_t&);
        const udp_sender_t &operator = (const udp_sender_t&);
    };

}

#endif
