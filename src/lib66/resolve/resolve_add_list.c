/*
 * resolve_add_list.c
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

#include <stdint.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>

int resolve_add_list(resolve_wrapper_t *wres, strbuf const *list, uint32_t *field)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf joined = STRBUF_ZERO ;

    if (list->len && (!strbuf_copy(&joined, list) || !sbl_rebuild_with_delim(&joined, ' ')))
        return -1 ;

    ssize_t off = resolve_add_string(wres, list->len ? joined.s : "") ;
    if (off < 0)
        return -1 ;

    *field = (uint32_t)off ;

    return 1 ;
}
