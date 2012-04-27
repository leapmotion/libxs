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

#ifndef __XS_ATOMIC_PTR_HPP_INCLUDED__
#define __XS_ATOMIC_PTR_HPP_INCLUDED__

#include <stddef.h>

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

    //  This class encapsulates several atomic operations on pointers.

    template <typename T> class atomic_ptr_t
    {
    public:

        //  Initialise atomic pointer
        inline atomic_ptr_t ()
        {
            ptr = NULL;
        }

        //  Destroy atomic pointer
        inline ~atomic_ptr_t ()
        {
        }

        //  Set value of atomic pointer in a non-threadsafe way
        //  Use this function only when you are sure that at most one
        //  thread is accessing the pointer at the moment.
        inline void set (T *ptr_)
        {
            this->ptr = ptr_;
        }

        //  Perform atomic 'exchange pointers' operation. Pointer is set
        //  to the 'val' value. Old value is returned.
        inline T *xchg (T *val_)
        {
#if (defined(__GNUC__) && defined(__ARM_ARCH_7A__))
            T* old;
            unsigned int flag;
            __asm__ volatile (
                "       dmb     sy\n\t"
                "1:     ldrex   %1, [%3]\n\t"
                "       strex   %0, %4, [%3]\n\t"
                "       teq     %0, #0\n\t"
                "       bne     1b\n\t"
                "       dmb     sy\n\t"
                : "=&r"(flag), "=&r"(old), "+Qo"(ptr)
                : "r"(&ptr), "r"(val_)
                : "cc");
            return old;

#elif defined(XS_ATOMIC_GCC_SYNC)
            {
                T* ov;
                do
                {
                    ov = const_cast<T*>(ptr);
                } while (!__sync_bool_compare_and_swap (&ptr, ov, val_));
                return ov;
            }

#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
            T *old;
            __asm__ volatile (
                "lock; xchg %0, %2"
                : "=r" (old), "=m" (ptr)
                : "m" (ptr), "0" (val_));
            return old;

#elif defined(XS_ATOMIC_SOLARIS)
            return (T*) atomic_swap_ptr (&ptr, val_);

#elif defined(XS_HAVE_WINDOWS)
            return (T*) InterlockedExchangePointer ((PVOID*) &ptr, val_);

#else
            sync.lock ();
            T *old = (T*) ptr;
            ptr = val_;
            sync.unlock ();
            return old;
#endif
        }

        //  Perform atomic 'compare and swap' operation on the pointer.
        //  The pointer is compared to 'cmp' argument and if they are
        //  equal, its value is set to 'val'. Old value of the pointer
        //  is returned.
        inline T *cas (T *cmp_, T *val_)
        {
#if (defined(__GNUC__) && defined(__ARM_ARCH_7A__))
            T *old;
            unsigned int flag;
            __asm__ volatile (
                "       dmb     sy\n\t"
                "1:     ldrex   %1, [%3]\n\t"
                "       mov     %0, #0\n\t"
                "       teq     %1, %4\n\t"
                "       strexeq %0, %5, [%3]\n\t"
                "       teq     %0, #0\n\t"
                "       bne     1b\n\t"
                "       dmb     sy\n\t"
                : "=&r"(flag), "=&r"(old), "+Qo"(ptr)
                : "r"(&ptr), "r"(cmp_), "r"(val_)
                : "cc");
            return old;

#elif defined(XS_ATOMIC_GCC_SYNC)
            return (T*) __sync_val_compare_and_swap (&ptr, cmp_, val_);

#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
            T *old;
            __asm__ volatile (
                "lock; cmpxchg %2, %3"
                : "=a" (old), "=m" (ptr)
                : "r" (val_), "m" (ptr), "0" (cmp_)
                : "cc");
            return old;

#elif defined(XS_ATOMIC_SOLARIS)
            return (T*) atomic_cas_ptr (&ptr, cmp_, val_);

#elif defined(XS_HAVE_WINDOWS)
            return (T*) InterlockedCompareExchangePointer (
                (volatile PVOID*) &ptr, val_, cmp_);

#else
            sync.lock ();
            T *old = (T*) ptr;
            if (ptr == cmp_)
                ptr = val_;
            sync.unlock ();
            return old;
#endif
        }

    private:

        volatile T *ptr;
#if defined XS_ATOMIC_OVER_MUTEX
        mutex_t sync;
#endif

        atomic_ptr_t (const atomic_ptr_t&);
        const atomic_ptr_t &operator = (const atomic_ptr_t&);
    };

}

//  Remove macros local to this file.
#if defined(XS_ATOMIC_OVER_MUTEX)
#   undef XS_ATOMIC_OVER_MUTEX
#endif

#endif
