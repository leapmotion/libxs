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

#include <vector>
#include <string>
#include <map>

#include "../include/xs/xs.h"

#include "topic_filter.hpp"
#include "err.hpp"

typedef std::vector <void*> subscribers_t;
typedef std::map <std::string, subscribers_t> topic_t;

static bool topic_match (const char *topic_,
    const unsigned char *data_, size_t size_)
{
    while (true) {

        //  Check whether matching is done.
        if (*topic_ == 0)
            return true;

        // Match one element.
        if (topic_ [0] == '*') {

            //  March wildcard element.
            ++topic_;
            while (size_ && *data_ != 0 && *data_ != '.')
                ++data_, --size_;
        }
        else {

            //  Match literal element.
            while (true) {
                if (topic_ [0] == '.' || topic_ [0] == 0)
                    break;
                if (!size_)
                    return false;
                if (topic_ [0] != *data_)
                    return false; 
                ++data_;
                --size_;
                ++topic_;
            }
        }

        //  Check whether matching is done.
        if (topic_ [0] == 0)
            return true;

        //  Match dot.
        if (topic_ [0] != '.')
            return false;  //  Malformed subscription, e.g. "*abc"
        if (!size_)
            return false;
        if (*data_ != '.')
            return false;
        ++data_;
        --size_;
        ++topic_;
    }
}

static int id (void *core_)
{
    return XS_FILTER_TOPIC;
}

static void *pf_create (void *core_)
{
    topic_t *pf = new (std::nothrow) topic_t;
    alloc_assert (pf);
    return (void*) pf;
}

static void pf_destroy (void *core_, void *pf_)
{
    xs_assert (pf_);
    delete (topic_t*) pf_;
}

static int pf_subscribe (void *core_, void *pf_, void *subscriber_,
    const unsigned char *data_, size_t size_)
{
    topic_t *self = (topic_t*) pf_;
    (*self) [std::string ((char*) data_, size_)].push_back (subscriber_);
    return xs_filter_subscribed (core_, data_, size_);
}

static int pf_unsubscribe (void *core_, void *pf_, void *subscriber_,
    const unsigned char *data_, size_t size_)
{
    topic_t *self = (topic_t*) pf_;

    topic_t::iterator it = self->find (std::string ((char*) data_, size_));
    if (it == self->end ()) {
        errno = EINVAL;
        return -1;
    }

    bool found = false;
    for (subscribers_t::iterator its = it->second.begin ();
          its != it->second.end (); ++its) {
        if (*its == subscriber_) {
            it->second.erase (its);
            found = true;
            break;
        }
    }
    if (!found) {
        errno = EINVAL;
        return -1;
    }

    if (it->second.empty ())
        self->erase (it);

    return 0;
}

static void pf_unsubscribe_all (void *core_, void *pf_, void *subscriber_)
{
    topic_t *self = (topic_t*) pf_;

    for (topic_t::iterator it = self->begin (); it != self->end ();) {
        for (subscribers_t::size_type i = 0; i < it->second.size (); ++i) {
            if (it->second [i] == subscriber_)
                it->second.erase (it->second.begin () + i--);
        }
        if (it->second.empty ())
            self->erase (it++);
        else
            ++it;
    }
}

static void pf_match (void *core_, void *pf_,
    const unsigned char *data_, size_t size_)
{
    topic_t *self = (topic_t*) pf_;
    for (topic_t::iterator it = self->begin (); it != self->end (); ++it) {
        if (topic_match (it->first.c_str (), data_, size_)) {
            for (subscribers_t::iterator its = it->second.begin ();
                  its != it->second.end (); ++its) {
                int rc = xs_filter_matching (core_, *its);
                errno_assert (rc == 0);
            }
        }
    }
}

static void *sf_create (void *core_)
{
    return pf_create (core_);
}

static void sf_destroy (void *core_, void *sf_)
{
    pf_destroy (core_, sf_);
}

static int sf_subscribe (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    return pf_subscribe (core_, sf_, NULL, data_, size_);
}

static int sf_unsubscribe (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    return pf_unsubscribe (core_, sf_, NULL, data_, size_);
}

static int sf_match (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    topic_t *self = (topic_t*) sf_;
    for (topic_t::iterator it = self->begin (); it != self->end (); ++it)
       if (topic_match (it->first.c_str (), data_, size_))
           return 1;
    return 0;
}

static xs_filter_t rgxp_filter = {
    XS_PLUGIN_FILTER,
    1,
    id,
    pf_create,
    pf_destroy,
    pf_subscribe,
    pf_unsubscribe,
    pf_unsubscribe_all,
    pf_match,
    sf_create,
    sf_destroy,
    sf_subscribe,
    sf_unsubscribe,
    sf_match
};

void *xs::topic_filter = (void*) &rgxp_filter;

