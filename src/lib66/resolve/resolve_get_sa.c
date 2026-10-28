/*
 * resolve_get_sa.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/cdb.h>
#include <oblibs/strbuf.h>

int resolve_get_sa(strbuf *sa, const ocdb *c)
{
    log_flow() ;

    ocdb_data cdata ;
    sa->len = 0 ;
    int r = ocdb_find(c, &cdata, "sa", 2) ;

    if (r < 0) {
        log_warnusys("search on cdb key: sa") ;
        return (errno = EINVAL, 0) ;
    }

    if (!r) {
        log_warn("unknown cdb key: sa") ;
        return (errno = EINVAL, 0) ;
    }

    if (!cdata.len)
        return (errno = EINVAL, 0) ;

    if (!strbuf_copyb(sa, cdata.s, cdata.len))
        return -1 ;

    return 1 ;
}
