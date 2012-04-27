/*
    Copyright (c) 2007-2009 iMatix Corporation
    Copyright (c) 2007-2011 Other contributors as noted in the AUTHORS file

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

#ifndef __XS_WIRE_HPP_INCLUDED__
#define __XS_WIRE_HPP_INCLUDED__

#include "stdint.hpp"

//  Protocol-related constants.

//  Protocol header.
#define SP_HEADER_LENGTH 8

//  Patterns.
#define SP_PAIR 1
#define SP_PUBSUB 2
#define SP_REQREP 3
#define SP_PIPELINE 4
#define SP_SURVEY 5

//  Roles.
#define SP_PAIR_PAIR 1
#define SP_PUBSUB_PUB 1
#define SP_PUBSUB_SUB 2
#define SP_REQREP_REQ 1
#define SP_REQREP_REP 2
#define SP_PIPELINE_PUSH 1
#define SP_PIPELINE_PULL 2
#define SP_SURVEY_SURVEYOR 1
#define SP_SURVEY_RESPONDENT 2

//  PUBSUB pattern commands.
#define SP_PUBSUB_CMD_SUBSCRIBE 1
#define SP_PUBSUB_CMD_UNSUBSCRIBE 2

namespace xs
{
    //  Protocol header type.
    typedef unsigned char sp_header_t [SP_HEADER_LENGTH];

    //  Get the SP protocol header for the specified pattern, version and role.
    inline void sp_get_header (sp_header_t header_, int pattern_, int version_,
        int role_)
    {
        header_ [0] = 0;
        header_ [1] = 0;
        header_ [2] = 'S';
        header_ [3] = 'P';
        header_ [4] = pattern_ & 0xff;
        header_ [5] = version_ & 0xff;
        header_ [6] = role_ & 0xff;
        header_ [7] = 0;
    }

    //  Helper functions to convert different integer types to/from network
    //  byte order.

    inline void put_uint8 (unsigned char *buffer_, uint8_t value)
    {
        *buffer_ = value;
    }

    inline uint8_t get_uint8 (unsigned char *buffer_)
    {
        return *buffer_;
    }

    inline void put_uint16 (unsigned char *buffer_, uint16_t value)
    {
        buffer_ [0] = (unsigned char) (((value) >> 8) & 0xff);
        buffer_ [1] = (unsigned char) (value & 0xff);
    }

    inline uint16_t get_uint16 (unsigned char *buffer_)
    {
        return
            (((uint16_t) buffer_ [0]) << 8) |
            ((uint16_t) buffer_ [1]);
    }

    inline void put_uint32 (unsigned char *buffer_, uint32_t value)
    {
        buffer_ [0] = (unsigned char) (((value) >> 24) & 0xff);
        buffer_ [1] = (unsigned char) (((value) >> 16) & 0xff);
        buffer_ [2] = (unsigned char) (((value) >> 8) & 0xff);
        buffer_ [3] = (unsigned char) (value & 0xff);
    }

    inline uint32_t get_uint32 (unsigned char *buffer_)
    {
        return
            (((uint32_t) buffer_ [0]) << 24) |
            (((uint32_t) buffer_ [1]) << 16) |
            (((uint32_t) buffer_ [2]) << 8) |
            ((uint32_t) buffer_ [3]);
    }

    inline void put_uint64 (unsigned char *buffer_, uint64_t value)
    {
        buffer_ [0] = (unsigned char) (((value) >> 56) & 0xff);
        buffer_ [1] = (unsigned char) (((value) >> 48) & 0xff);
        buffer_ [2] = (unsigned char) (((value) >> 40) & 0xff);
        buffer_ [3] = (unsigned char) (((value) >> 32) & 0xff);
        buffer_ [4] = (unsigned char) (((value) >> 24) & 0xff);
        buffer_ [5] = (unsigned char) (((value) >> 16) & 0xff);
        buffer_ [6] = (unsigned char) (((value) >> 8) & 0xff);
        buffer_ [7] = (unsigned char) (value & 0xff);
    }

    inline uint64_t get_uint64 (unsigned char *buffer_)
    {
        return
            (((uint64_t) buffer_ [0]) << 56) |
            (((uint64_t) buffer_ [1]) << 48) |
            (((uint64_t) buffer_ [2]) << 40) |
            (((uint64_t) buffer_ [3]) << 32) |
            (((uint64_t) buffer_ [4]) << 24) |
            (((uint64_t) buffer_ [5]) << 16) |
            (((uint64_t) buffer_ [6]) << 8) |
            ((uint64_t) buffer_ [7]);
    }

}

#endif
