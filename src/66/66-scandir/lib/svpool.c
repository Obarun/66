/*
 * svpool.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * Free-list index allocator (replaces skalibs genset). new() pops a free index in
 * O(1); delete() pushes it back. An inline oblibs bitset32_t (occupancy) drives a
 * delete-safe iteration by storage position (bounded by a high-water mark), so the
 * iterator callback may remove the element it is looking at -- exactly what
 * s6-svscan's scan() does through genset_iter + remove_deadinactive_iter.
 */

#include <errno.h>

#include <oblibs/bits.h>

#include "scandir.h"

int svpool_init(svpool_t *p, uint32_t *freelist, uint32_t n)
{
    if (!p || !freelist || !n)
        return (errno = EINVAL, 0) ;

    p->used = bitset32_init(n) ;

    if (!p->used.size)
        return (errno = EINVAL, 0) ; // n == 0 or n > UINT32_BITS_MAX (1024)

    p->freelist = freelist ;
    p->n = n ;
    p->freetop = n ;

    // store indices in descending order so popping from the top hands out 0,1,2,...
    for (uint32_t k = 0 ; k < n ; k++)
        p->freelist[k] = n - 1u - k ;

    p->high = 0 ;

    return 1 ;
}

uint32_t svpool_new(svpool_t *p)
{
    if (!p->freetop)
        return p->n ; // full

    uint32_t i = p->freelist[--p->freetop] ;

    bitset32_set(&p->used, i) ;

    if (i + 1u > p->high)
        p->high = i + 1u ;

    return i ;
}

void svpool_delete(svpool_t *p, uint32_t i)
{
    if (i >= p->n || !bitset32_isvalid(&p->used, i))
        return ; // out of range or double free

    bitset32_clear(&p->used, i) ;

    p->freelist[p->freetop++] = i ;
}

void svpool_iter(svpool_t *p, int (*f)(uint32_t i, void *aux), void *aux)
{
    for (uint32_t i = 0 ; i < p->high ; i++) {
        if (bitset32_isvalid(&p->used, i)) {
            if (!f(i, aux))
                break ;
        }
    }
}
