/*
    Copyright (c) 2009-2012 250bpm s.r.o.
    Copyright (c) 2007-2009 iMatix Corporation
    Copyright (c) 2007-2012 Other contributors as noted in the AUTHORS file

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

#ifndef __XS_ATOMIC_COUNTER_HPP_INCLUDED__
#define __XS_ATOMIC_COUNTER_HPP_INCLUDED__

#include "stdint.hpp"
#include "platform.hpp"

#if defined(XS_ATOMIC_GCC_SYNC)
#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
#elif (defined(__GNUC__) && defined(__ARM_ARCH_7A__))
#elif defined(XS_ATOMIC_SOLARIS)
#   include <atomic.h>
#elif defined(XS_HAVE_WINDOWS)
#   include "windows.hpp"
#else
#   define XS_ATOMIC_OVER_MUTEX 1
#   include "mutex.hpp"
#endif

namespace xs
{

    //  This class represents an integer that can be incremented/decremented
    //  in atomic fashion.

    class atomic_counter_t
    {
    public:

        typedef uint32_t integer_t;

        inline atomic_counter_t (integer_t value_ = 0) :
            value (value_)
        {
        }

        inline ~atomic_counter_t ()
        {
        }

        //  Set counter value (not thread-safe).
        inline void set (integer_t value_)
        {
            value = value_;
        }

        //  Atomic addition. Returns the old value.
        inline integer_t add (integer_t increment_)
        {
            integer_t old_value;

#if (defined(__GNUC__) && defined(__ARM_ARCH_7A__))
            integer_t flag, tmp;
            __asm__ volatile (
                "       dmb     sy\n\t"
                "1:     ldrex   %0, [%5]\n\t"
                "       add     %2, %0, %4\n\t"
                "       strex   %1, %2, [%5]\n\t"
                "       teq     %1, #0\n\t"
                "       bne     1b\n\t"
                "       dmb     sy\n\t"
                : "=&r"(old_value), "=&r"(flag), "=&r"(tmp), "+Qo"(value)
                : "Ir"(increment_), "r"(&value)
                : "cc");

#elif defined(XS_ATOMIC_GCC_SYNC)
            old_value = __sync_fetch_and_add (&value, increment_);

#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
            __asm__ volatile (
                "lock; xadd %0, %1 \n\t"
                : "=r" (old_value), "=m" (value)
                : "0" (increment_), "m" (value)
                : "cc", "memory");

#elif defined(XS_ATOMIC_SOLARIS)
            integer_t new_value = atomic_add_32_nv (&value, increment_);
            old_value = new_value - increment_;

#elif defined(XS_HAVE_WINDOWS)
            old_value = InterlockedExchangeAdd ((LONG*) &value, increment_);

#else
            sync.lock ();
            old_value = value;
            value += increment_;
            sync.unlock ();
#endif
            return old_value;
        }

        //  Atomic subtraction. Returns false if the counter drops to zero.
        inline bool sub (integer_t decrement)
        {
#if (defined(__GNUC__) && defined(__ARM_ARCH_7A__))
            integer_t old_value, flag, tmp;
            __asm__ volatile (
                "       dmb     sy\n\t"
                "1:     ldrex   %0, [%5]\n\t"
                "       sub     %2, %0, %4\n\t"
                "       strex   %1, %2, [%5]\n\t"
                "       teq     %1, #0\n\t"
                "       bne     1b\n\t"
                "       dmb     sy\n\t"
                : "=&r"(old_value), "=&r"(flag), "=&r"(tmp), "+Qo"(value)
                : "Ir"(decrement), "r"(&value)
                : "cc");
            return old_value - decrement != 0;

#elif defined(XS_ATOMIC_GCC_SYNC)
            integer_t new_value = __sync_sub_and_fetch (&value, decrement);
            return (new_value != 0);

#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
            integer_t oldval = -decrement;
            volatile integer_t *val = &value;
            __asm__ volatile ("lock; xaddl %0,%1"
                : "=r" (oldval), "=m" (*val)
                : "0" (oldval), "m" (*val)
                : "cc", "memory");
            return oldval != decrement;

#elif defined(XS_ATOMIC_SOLARIS)
            int32_t delta = - ((int32_t) decrement);
            integer_t nv = atomic_add_32_nv (&value, delta);
            return nv != 0;

#elif defined(XS_HAVE_WINDOWS)
            LONG delta = - ((LONG) decrement);
            integer_t old = InterlockedExchangeAdd ((LONG*) &value, delta);
            return old - decrement != 0;

#else
            sync.lock ();
            value -= decrement;
            bool result = value ? true : false;
            sync.unlock ();
            return result;
#endif
        }

        inline integer_t get ()
        {
            return value;
        }

    private:

        volatile integer_t value;
#if defined(XS_ATOMIC_OVER_MUTEX)
        mutex_t sync;
#endif

        atomic_counter_t (const atomic_counter_t&);
        const atomic_counter_t& operator = (const atomic_counter_t&);
    };

}

//  Remove macros local to this file.
#if defined(XS_ATOMIC_OVER_MUTEX)
#   undef XS_ATOMIC_OVER_MUTEX
#endif

#endif

