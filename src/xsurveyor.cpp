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

#include "xsurveyor.hpp"
#include "err.hpp"
#include "msg.hpp"

xs::xsurveyor_t::xsurveyor_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_)
{
    options.type = XS_XSURVEYOR;

    //  When the XSURVEYOR socket is close it makes no sense to send any pending
    //  surveys. The responses will be unroutable anyway.
    options.delay_on_close = false;
}

xs::xsurveyor_t::~xsurveyor_t ()
{
}

void xs::xsurveyor_t::xattach_pipe (pipe_t *pipe_, bool icanhasall_)
{
    xs_assert (pipe_);
    fq.attach (pipe_);
    dist.attach (pipe_);
}

int xs::xsurveyor_t::xsend (msg_t *msg_, int flags_)
{
    return dist.send_to_all (msg_, flags_);
}

int xs::xsurveyor_t::xrecv (msg_t *msg_, int flags_)
{
    return fq.recv (msg_, flags_);
}

bool xs::xsurveyor_t::xhas_in ()
{
    return fq.has_in ();
}

bool xs::xsurveyor_t::xhas_out ()
{
    return dist.has_out ();
}

void xs::xsurveyor_t::xread_activated (pipe_t *pipe_)
{
    fq.activated (pipe_);
}

void xs::xsurveyor_t::xwrite_activated (pipe_t *pipe_)
{
    dist.activated (pipe_);
}

void xs::xsurveyor_t::xterminated (pipe_t *pipe_)
{
    fq.terminated (pipe_);
    dist.terminated (pipe_);
}

xs::xsurveyor_session_t::xsurveyor_session_t (io_thread_t *io_thread_,
      bool connect_, socket_base_t *socket_, const options_t &options_,
      const char *protocol_, const char *address_) :
    session_base_t (io_thread_, connect_, socket_, options_, protocol_,
        address_)
{
}

xs::xsurveyor_session_t::~xsurveyor_session_t ()
{
}

