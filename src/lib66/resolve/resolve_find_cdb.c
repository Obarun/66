/*
 * resolve_find_cdb.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/types.h>

#include <66/resolve.h>

int resolve_find_cdb(stralloc *result, cdb const *c, char const *key)
{
    //log_flow() ;

    uint32_t x = 0 ;
    size_t klen = strlen(key) ;
    cdb_data cdata ;

    result->len = 0 ;

    int r = cdb_find(c, &cdata, key, klen) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_LESSONE,"search on cdb key: ",key) ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"unknown cdb key: ",key) ;

    char pack[cdata.len + 1] ;
    memcpy(pack,cdata.s, cdata.len) ;
    pack[cdata.len] = 0 ;

    uint32_unpack_big(pack, &x) ;

    if (!auto_stra(result,pack))
        log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

    return x ;
}

