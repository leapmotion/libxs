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

#ifndef __XS_UDP_RECEIVER_HPP_INCLUDED__
#define __XS_UDP_RECEIVER_HPP_INCLUDED__

#include "platform.hpp"

#include "io_object.hpp"
#include "i_engine.hpp"
#include "options.hpp"
#include "decoder.hpp"
#include "address.hpp"

namespace xs
{

    class io_thread_t;
    class session_base_t;

    class udp_receiver_t : public io_object_t, public i_engine
    {

    public:

        udp_receiver_t (xs::io_thread_t *parent_, const options_t &options_);
        ~udp_receiver_t ();

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

    private:

        //  We don't support forwarding subscriptions upstream (yet).
        void drop_subscriptions ();

        //  Underlying UDP socket.
        fd_t socket;
        handle_t socket_handle;

        //  Socket address.
        address_t address;

        //  Socket options.
        options_t options;

        //  Decoder for this socket.
        decoder_t *decoder;

        //  Amount of unprocessed data waiting in decoder, if any.
        ssize_t pending_bytes;

        //  Pointer to unprocessed data in buffer, if any.
        unsigned char *pending_p;

        //  Buffer for data to process.
        unsigned char data [pgm_max_tpdu];

        //  Associated session.
        xs::session_base_t *session;

        //  UDP header size.
        static const size_t udp_header_size = 6;

        //  Last sequence number seen, 0 if none.
        uint32_t last_seq_no;

        udp_receiver_t (const udp_receiver_t&);
        const udp_receiver_t &operator = (const udp_receiver_t&);
    };

}

#endif
