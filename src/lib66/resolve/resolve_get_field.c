/*
 * resolve_get_field.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>

int resolve_get_field(strbuf *sa, resolve_wrapper_t_ref wres, char const *base, char const *name, resolve_enum_table_t table)
{
    log_flow() ;

    if (!name)
        return (errno = ENOENT, 0) ;

    int r = resolve_read(wres, base, name) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        /** an addon (any non-core type) may be absent: ENOENT reads the
         * defaults of its ZERO struct, an empty string at every offset. The
         * core (DATA_SERVICE) must exist, and a file that does not read is no
         * default. */
        if (errno != ENOENT || wres->type == DATA_SERVICE || wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER)
            return 0 ;
    }

    if (!resolve_get_field_from(sa, wres, table))
        return -1 ;

    if (sa->len) {
        /** sbl_clean_string forbids aliasing its source with the destination
         * buffer: clean into a separate strbuf, then copy back. */
        _cleanup_strbuf_ strbuf clean = STRBUF_ZERO ;
        if (!sbl_clean_string(&clean, sa->s) || !strbuf_copy(sa, &clean))
            return -1 ;
    }

    return 1 ;
}
