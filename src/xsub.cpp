/*
    Copyright (c) 2010-2012 250bpm s.r.o.
    Copyright (c) 2011 VMware, Inc.
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

#include <string.h>

#include "xsub.hpp"
#include "err.hpp"
#include "wire.hpp"

xs::xsub_t::xsub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_)
{
    options.type = XS_XSUB;

    //  When socket is being closed down we don't want to wait till pending
    //  subscription commands are sent to the wire.
    options.linger = 0;

    //  Also, we want the subscription buffer to be elastic by default.
    options.sndhwm = 0;
}

xs::xsub_t::~xsub_t ()
{
}

void xs::xsub_t::xattach_pipe (pipe_t *pipe_, bool icanhasall_)
{
    xs_assert (pipe_);
    fq.attach (pipe_);

    //  Pipes with 0MQ/2.1-style protocol are not eligible for accepting
    //  subscriptions.
    if (pipe_->get_protocol () == 1)
        return;

    dist.attach (pipe_);

    //  Send all the cached subscriptions to the new upstream peer.
    for (subscriptions_t::iterator its = subscriptions.begin ();
          its != subscriptions.end (); ++its)
        send_subscription (pipe_, true, its->first.first,
            its->first.second.data (), its->first.second.size ());
    pipe_->flush ();
}

void xs::xsub_t::xread_activated (pipe_t *pipe_)
{
    fq.activated (pipe_);
}

void xs::xsub_t::xwrite_activated (pipe_t *pipe_)
{
    dist.activated (pipe_);
}

void xs::xsub_t::xterminated (pipe_t *pipe_)
{
    fq.terminated (pipe_);
    if (pipe_->get_protocol () != 1)
        dist.terminated (pipe_);
}

void xs::xsub_t::xhiccuped (pipe_t *pipe_)
{
    //  In 0MQ/2.1 protocol there is no subscription forwarding.
    if (pipe_->get_protocol () == 1)
        return;

    //  Send all the cached subscriptions to the new upstream peer.
    for (subscriptions_t::iterator its = subscriptions.begin ();
          its != subscriptions.end (); ++its)
        send_subscription (pipe_, true, its->first.first,
            its->first.second.data (), its->first.second.size ());
    pipe_->flush ();
}

int xs::xsub_t::xsend (msg_t *msg_, int flags_)
{
    size_t size = msg_->size ();
    unsigned char *data = (unsigned char*) msg_->data ();

    if (size < 4) {
        errno = EINVAL;
        return -1;
    }
    int cmd = get_uint16 (data);
    int filter_id = get_uint16 (data + 2);
    if (cmd != XS_CMD_SUBSCRIBE && cmd != XS_CMD_UNSUBSCRIBE) {
        errno = EINVAL;
        return -1;
    }

    //  Process the subscription.
    if (cmd == XS_CMD_SUBSCRIBE) {
        subscriptions_t::iterator it = subscriptions.insert (
           std::make_pair (std::make_pair (filter_id,
           blob_t (data + 4, size - 4)), 0)).first;
        ++it->second;
        if (it->second == 1)
            return dist.send_to_all (msg_, flags_);
    }
    else if (cmd == XS_CMD_UNSUBSCRIBE) {
        subscriptions_t::iterator it = subscriptions.find (
            std::make_pair (filter_id, blob_t (data + 4, size - 4)));
        if (it != subscriptions.end ()) {
            xs_assert (it->second);
            --it->second;
            if (!it->second) {
                subscriptions.erase (it);
                return dist.send_to_all (msg_, flags_);
            }
        }
    }

    int rc = msg_->close ();
    errno_assert (rc == 0);
    rc = msg_->init ();
    errno_assert (rc == 0);
    return 0;
}

bool xs::xsub_t::xhas_out ()
{
    //  Subscription can be added/removed anytime.
    return true;
}

int xs::xsub_t::xrecv (msg_t *msg_, int flags_)
{
    return fq.recv (msg_, flags_);
}

bool xs::xsub_t::xhas_in ()
{
    return fq.has_in ();
}

void xs::xsub_t::send_subscription (pipe_t *pipe_, bool subscribe_,
    int filter_id_, const unsigned char *data_, size_t size_)
{
    //  Create the subsctription message.
    msg_t msg;
    int rc = msg.init_size (size_ + 4);
    errno_assert (rc == 0);
    unsigned char *data = (unsigned char*) msg.data ();
    put_uint16 (data, subscribe_ ? XS_CMD_SUBSCRIBE : XS_CMD_UNSUBSCRIBE);
    put_uint16 (data + 2, filter_id_);
    memcpy (data + 4, data_, size_);

    //  Send it to the pipe.
    bool sent = pipe_->write (&msg);

    //  If we reached the SNDHWM, and thus cannot send the subscription, drop
    //  the subscription message instead. This matches the behaviour of
    //  xs_setsockopt(XS_SUBSCRIBE, ...), which also drops subscriptions
    //  when the SNDHWM is reached.
    if (!sent)
        msg.close ();
}

xs::xsub_session_t::xsub_session_t (io_thread_t *io_thread_, bool connect_,
      socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    session_base_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::xsub_session_t::~xsub_session_t ()
{
}

