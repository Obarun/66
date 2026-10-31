/*
 * resolve_add_string.c
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
 */

/* The contract of resolve_add_string(), by effect: the string is appended to
 * the sa of the resolve with its terminating zero and its offset returned, an
 * empty string for a null @data. */

#include <assert.h>
#include <string.h>

#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>

int main(void)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
    ssize_t first, off ;

    resolve_init(&w) ;

    first = resolve_add_string(&w, "first") ;
    assert(first >= 0) ;
    assert(!strcmp(res.sa.s + first, "first")) ;

    off = resolve_add_string(&w, 0) ;
    assert(off == first + 6) ;
    assert(res.sa.s[off] == 0) ;

    resolve_free_obj(&w) ;

    return 0 ;
}
