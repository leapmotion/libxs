/*
    Copyright (c) 2012 250bpm s.r.o.
    Copyright (c) 2011-2012 Spotify AB
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

#include <new>
#include <map>
#include <stddef.h>
#include <stdlib.h>

#include "../include/xs/xs.h"

#include "prefix_filter.hpp"
#include "err.hpp"

struct pfx_node_t
{
    //  Pointer to particular subscriber associated with the reference count.
    typedef std::map <void*, int> subscribers_t;
    subscribers_t *subscribers;

    unsigned char min;
    unsigned short count;
    unsigned short live_nodes;
    union {
        struct pfx_node_t *node;
        struct pfx_node_t **table;
    } next;
};

static bool pfx_is_redundant (pfx_node_t *node_)
{
    return !node_->subscribers && node_->live_nodes == 0;
}

static void pfx_init (pfx_node_t *node_)
{
    node_->subscribers = NULL;
    node_->min = 0;
    node_->count = 0;
    node_->live_nodes = 0;
}

static void pfx_close (pfx_node_t *node_)
{
    if (node_->subscribers) {
        delete node_->subscribers;
        node_->subscribers = NULL;
    }

    if (node_->count == 1) {
        xs_assert (node_->next.node);
        pfx_close (node_->next.node);
        free (node_->next.node);
        node_->next.node = NULL;
    }
    else if (node_->count > 1) {
        for (unsigned short i = 0; i != node_->count; ++i)
            if (node_->next.table [i]) {
                pfx_close (node_->next.table [i]);
                free (node_->next.table [i]);
            }
        free (node_->next.table);
    }
}

static bool pfx_add (pfx_node_t *node_,
    const unsigned char *prefix_, size_t size_, void *subscriber_)
{
    //  We are at the node corresponding to the prefix. We are done.
    if (!size_) {
        bool result = !node_->subscribers;
        if (!node_->subscribers)
            node_->subscribers = new (std::nothrow) pfx_node_t::subscribers_t;
        pfx_node_t::subscribers_t::iterator it = node_->subscribers->insert (
            pfx_node_t::subscribers_t::value_type (subscriber_, 0)).first;
        ++it->second;
        return result;
    }

    unsigned char c = *prefix_;
    if (c < node_->min || c >= node_->min + node_->count) {

        //  The character is out of range of currently handled
        //  charcters. We have to extend the table.
        if (!node_->count) {
            node_->min = c;
            node_->count = 1;
            node_->next.node = NULL;
        }
        else if (node_->count == 1) {
            unsigned char oldc = node_->min;
            pfx_node_t *oldp = node_->next.node;
            node_->count =
                (node_->min < c ? c - node_->min : node_->min - c) + 1;
            node_->next.table = (pfx_node_t**)
                malloc (sizeof (pfx_node_t*) * node_->count);
            alloc_assert (node_->next.table);
            for (unsigned short i = 0; i != node_->count; ++i)
                node_->next.table [i] = 0;
            node_->min = std::min (node_->min, c);
            node_->next.table [oldc - node_->min] = oldp;
        }
        else if (node_->min < c) {

            //  The new character is above the current character range.
            unsigned short old_count = node_->count;
            node_->count = c - node_->min + 1;
            node_->next.table =
                (pfx_node_t**) realloc ((void*) node_->next.table,
                sizeof (pfx_node_t*) * node_->count);
            xs_assert (node_->next.table);
            for (unsigned short i = old_count; i != node_->count; i++)
                node_->next.table [i] = NULL;
        }
        else {

            //  The new character is below the current character range.
            unsigned short old_count = node_->count;
            node_->count = (node_->min + old_count) - c;
            node_->next.table =
                (pfx_node_t**) realloc ((void*) node_->next.table,
                sizeof (pfx_node_t*) * node_->count);
            xs_assert (node_->next.table);
            memmove (node_->next.table + node_->min - c, node_->next.table,
                old_count * sizeof (pfx_node_t*));
            for (unsigned short i = 0; i != node_->min - c; i++)
                node_->next.table [i] = NULL;
            node_->min = c;
        }
    }

    //  If next node does not exist, create one.
    if (node_->count == 1) {
        if (!node_->next.node) {
            node_->next.node = (pfx_node_t*) malloc (sizeof (pfx_node_t));
            alloc_assert (node_->next.node);
            pfx_init (node_->next.node);
            ++node_->live_nodes;
            xs_assert (node_->next.node);
        }
        return pfx_add (node_->next.node, prefix_ + 1, size_ - 1, subscriber_);
    }
    else {
        if (!node_->next.table [c - node_->min]) {
            node_->next.table [c - node_->min] =
                (pfx_node_t*) malloc (sizeof (pfx_node_t));
            alloc_assert (node_->next.table [c - node_->min]);
            pfx_init (node_->next.table [c - node_->min]);
            ++node_->live_nodes;
            xs_assert (node_->next.table [c - node_->min]);
        }
        return pfx_add (node_->next.table [c - node_->min],
            prefix_ + 1, size_ - 1, subscriber_);
    }
}

static void pfx_rm_all (pfx_node_t *node_, void *subscribers_,
    unsigned char **buff_, size_t buffsize_, size_t maxbuffsize_, void *arg_)
{
    //  Remove the subscription from this node.
    if (node_->subscribers) {
        pfx_node_t::subscribers_t::iterator it =
            node_->subscribers->find (subscribers_);
        if (it != node_->subscribers->end ()) {
            xs_assert (it->second);
            --it->second;
            if (!it->second) {
                node_->subscribers->erase (it);
                if (node_->subscribers->empty ()) {
                    int rc = xs_filter_unsubscribed (arg_, *buff_, buffsize_);
                    errno_assert (rc == 0);
                    delete node_->subscribers;
                    node_->subscribers = 0;
                }
            }
        }
    }

    //  Adjust the buffer.
    if (buffsize_ >= maxbuffsize_) {
        maxbuffsize_ = buffsize_ + 256;
        *buff_ = (unsigned char*) realloc (*buff_, maxbuffsize_);
        alloc_assert (*buff_);
    }

    //  If there are no subnodes in the trie, return.
    if (node_->count == 0)
        return;

    //  If there's one subnode (optimisation).
    if (node_->count == 1) {
        (*buff_) [buffsize_] = node_->min;
        buffsize_++;
        pfx_rm_all (node_->next.node, subscribers_, buff_, buffsize_,
            maxbuffsize_, arg_);

        //  Prune the node if it was made redundant by the removal
        if (pfx_is_redundant (node_->next.node)) {
            pfx_close (node_->next.node);
            free (node_->next.node);
            node_->next.node = 0;
            node_->count = 0;
            --node_->live_nodes;
            xs_assert (node_->live_nodes == 0);
        }
        return;
    }

    //  If there are multiple subnodes.

    //  New min non-null character in the node table after the removal.
    unsigned char new_min = node_->min + node_->count - 1;

    //  New max non-null character in the node table after the removal.
    unsigned char new_max = node_->min;
    for (unsigned short c = 0; c != node_->count; c++) {
        (*buff_) [buffsize_] = node_->min + c;
        if (node_->next.table [c]) {
            pfx_rm_all (node_->next.table [c], subscribers_, buff_,
                buffsize_ + 1, maxbuffsize_, arg_);

            //  Prune redundant nodes from the the trie.
            if (pfx_is_redundant (node_->next.table [c])) {
                pfx_close (node_->next.table [c]);
                free (node_->next.table [c]);
                node_->next.table [c] = 0;

                xs_assert (node_->live_nodes > 0);
                --node_->live_nodes;
            }
            else {
                //  The node is not redundant, so it's a candidate for being
                //  the new min/max node.
                //
                //  We loop through the node array from left to right, so the
                //  first non-null, non-redundant node encountered is the new
                //  minimum index. Conversely, the last non-redundant, non-null
                //  node encountered is the new maximum index.
                if (c + node_->min < new_min)
                    new_min = c + node_->min;
                if (c + node_->min > new_max)
                    new_max = c + node_->min;
            }
        }
    }

    xs_assert (node_->count > 1);

    //  Compact the node table if possible.
    if (node_->live_nodes == 1) {

        //  If there's only one live node in the table we can
        //  switch to using the more compact single-node
        //  representation.
        xs_assert (new_min == new_max);
        xs_assert (new_min >= node_->min &&
            new_min < node_->min + node_->count);
        pfx_node_t *node = node_->next.table [new_min - node_->min];
        xs_assert (node);
        free (node_->next.table);
        node_->next.node = node;
        node_->count = 1;
        node_->min = new_min;
    }
    else if (node_->live_nodes > 1 &&
          (new_min > node_->min || new_max < node_->min + node_->count - 1)) {
        xs_assert (new_max - new_min + 1 > 1);

        pfx_node_t **old_table = node_->next.table;
        xs_assert (new_min > node_->min ||
            new_max < node_->min + node_->count - 1);
        xs_assert (new_min >= node_->min);
        xs_assert (new_max <= node_->min + node_->count - 1);
        xs_assert (new_max - new_min + 1 < node_->count);

        node_->count = new_max - new_min + 1;
        node_->next.table =
            (pfx_node_t**) malloc (sizeof (pfx_node_t*) * node_->count);
        alloc_assert (node_->next.table);

        memmove (node_->next.table, old_table + (new_min - node_->min),
                 sizeof (pfx_node_t*) * node_->count);
        free (old_table);

        node_->min = new_min;
    }
}

static bool pfx_rm (pfx_node_t *node_, const unsigned char *prefix_,
    size_t size_, void *subscriber_)
{
    if (!size_) {

        //  Remove the subscription from this node.
        if (node_->subscribers) {
            pfx_node_t::subscribers_t::iterator it =
                node_->subscribers->find (subscriber_);
            if (it != node_->subscribers->end ()) {
                xs_assert (it->second);
                --it->second;
                if (!it->second) {
                    node_->subscribers->erase (it);
                    if (node_->subscribers->empty ()) {
                        delete node_->subscribers;
                        node_->subscribers = 0;
                    }
                }
            }
        }
        return !node_->subscribers;
    }

    unsigned char c = *prefix_;
    if (!node_->count || c < node_->min || c >= node_->min + node_->count)
        return false;

    pfx_node_t *next_node = node_->count == 1 ? node_->next.node :
        node_->next.table [c - node_->min];

    if (!next_node)
        return false;

    bool ret = pfx_rm (next_node, prefix_ + 1, size_ - 1, subscriber_);

    if (pfx_is_redundant (next_node)) {
        pfx_close (next_node);
        free (next_node);
        xs_assert (node_->count > 0);

        if (node_->count == 1) {
            node_->next.node = 0;
            node_->count = 0;
            --node_->live_nodes;
            xs_assert (node_->live_nodes == 0);
        }
        else {
            node_->next.table [c - node_->min] = 0;
            xs_assert (node_->live_nodes > 1);
            --node_->live_nodes;

            //  Compact the table if possible.
            if (node_->live_nodes == 1) {
                //  If there's only one live node in the table we can
                //  switch to using the more compact single-node
                //  representation
                pfx_node_t *node = 0;
                for (unsigned short i = 0; i < node_->count; ++i) {
                    if (node_->next.table [i]) {
                        node = node_->next.table [i];
                        node_->min += i;
                        break;
                    }
                }

                xs_assert (node);
                free (node_->next.table);
                node_->next.node = node;
                node_->count = 1;
            }
            else if (c == node_->min) {

                //  We can compact the table "from the left".
                unsigned char new_min = node_->min;
                for (unsigned short i = 1; i < node_->count; ++i) {
                    if (node_->next.table [i]) {
                        new_min = i + node_->min;
                        break;
                    }
                }
                xs_assert (new_min != node_->min);

                pfx_node_t **old_table = node_->next.table;
                xs_assert (new_min > node_->min);
                xs_assert (node_->count > new_min - node_->min);

                node_->count = node_->count - (new_min - node_->min);
                node_->next.table = (pfx_node_t**)
                    malloc (sizeof (pfx_node_t*) * node_->count);
                alloc_assert (node_->next.table);

                memmove (node_->next.table, old_table + (new_min - node_->min),
                         sizeof (pfx_node_t*) * node_->count);
                free (old_table);

                node_->min = new_min;
            }
            else if (c == node_->min + node_->count - 1) {

                //  We can compact the table "from the right".
                unsigned short new_count = node_->count;
                for (unsigned short i = 1; i < node_->count; ++i) {
                    if (node_->next.table [node_->count - 1 - i]) {
                        new_count = node_->count - i;
                        break;
                    }
                }
                xs_assert (new_count != node_->count);
                node_->count = new_count;

                pfx_node_t **old_table = node_->next.table;
                node_->next.table = (pfx_node_t**)
                    malloc (sizeof (pfx_node_t*) * node_->count);
                alloc_assert (node_->next.table);

                memmove (node_->next.table, old_table,
                    sizeof (pfx_node_t*) * node_->count);
                free (old_table);
            }
        }
    }

    return ret;
}

//  Implementation of the public filter interface.

static int id (void *core_)
{
    return XS_FILTER_PREFIX;
}

static void *pf_create (void *core_)
{
    pfx_node_t *root = (pfx_node_t*) malloc (sizeof (pfx_node_t));
    alloc_assert (root);
    pfx_init (root);
    return (void*) root;
}

static void pf_destroy (void *core_, void *pf_)
{
    pfx_close ((pfx_node_t*) pf_);
    free (pf_);
}

static int pf_subscribe (void *core_, void *pf_, void *subscriber_,
    const unsigned char *data_, size_t size_)
{
    return pfx_add ((pfx_node_t*) pf_, data_, size_, subscriber_) ? 1 : 0;
}

static int pf_unsubscribe (void *core_, void *pf_, void *subscriber_,
    const unsigned char *data_, size_t size_)
{
    return pfx_rm ((pfx_node_t*) pf_, data_, size_, subscriber_) ? 1 : 0;
}

static void pf_unsubscribe_all (void *core_, void *pf_, void *subscriber_)
{
    unsigned char *buff = NULL;
    pfx_rm_all ((pfx_node_t*) pf_, subscriber_, &buff, 0, 0, core_);
    free (buff);
}

static void pf_match (void *core_, void *pf_,
    const unsigned char *data_, size_t size_)
{
    pfx_node_t *current = (pfx_node_t*) pf_;
    while (true) {

        //  Signal the subscribers attached to this node.
        if (current->subscribers) {
            for (pfx_node_t::subscribers_t::iterator it =
                  current->subscribers->begin ();
                  it != current->subscribers->end (); ++it) {
                int rc = xs_filter_matching (core_, it->first);
                errno_assert (rc == 0);
            }
        }

        //  If we are at the end of the message, there's nothing more to match.
        if (!size_)
            break;

        //  If there are no subnodes in the trie, return.
        if (current->count == 0)
            break;

        //  If there's one subnode (optimisation).
		if (current->count == 1) {
            if (data_ [0] != current->min)
                break;
            current = current->next.node;
            data_++;
            size_--;
		    continue;
		}

		//  If there are multiple subnodes.
        if (data_ [0] < current->min || data_ [0] >=
              current->min + current->count)
            break;
        if (!current->next.table [data_ [0] - current->min])
            break;
        current = current->next.table [data_ [0] - current->min];
        data_++;
        size_--;
    }
}

static void *sf_create (void *core_)
{
    pfx_node_t *root = (pfx_node_t*) malloc (sizeof (pfx_node_t));
    alloc_assert (root);
    pfx_init (root);
    return (void*) root;
}

static void sf_destroy (void *core_, void *sf_)
{
    pfx_close ((pfx_node_t*) sf_);
    free (sf_);
}

static int sf_subscribe (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    if (pfx_add ((pfx_node_t*) sf_, data_, size_, NULL))
        return xs_filter_subscribed (core_, data_, size_);
    return 0;
}

static int sf_unsubscribe (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    if (pfx_rm ((pfx_node_t*) sf_, data_, size_, NULL))
        return xs_filter_unsubscribed (core_, data_, size_);
    return 0;
}

static int sf_match (void *core_, void *sf_,
    const unsigned char *data_, size_t size_)
{
    //  This function is on critical path. It deliberately doesn't use
    //  recursion to get a bit better performance.
    pfx_node_t *current = (pfx_node_t*) sf_;
    while (true) {

        //  We've found a corresponding subscription!
        if (current->subscribers)
            return 1;

        //  We've checked all the data and haven't found matching subscription.
        if (!size_)
            return 0;

        //  If there's no corresponding slot for the first character
        //  of the prefix, the message does not match.
        unsigned char c = *data_;
        if (c < current->min || c >= current->min + current->count)
            return 0;

        //  Move to the next character.
        if (current->count == 1)
            current = current->next.node;
        else {
            current = current->next.table [c - current->min];
            if (!current)
                return 0;
        }
        data_++;
        size_--;
    }
}

static xs_filter_t pfx_filter = {
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

void *xs::prefix_filter = (void*) &pfx_filter;

