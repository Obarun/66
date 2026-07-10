/*
 * resolve_get_key.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/cdb.h>
#include <oblibs/types.h>

int resolve_get_key_u32(const ocdb *c, const char *key, uint32_t *field)
{
    ocdb_data cdata ;

    int r = ocdb_find(c, &cdata, key, strlen(key)) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_ZERO, "search on cdb key: ", key) ;
    if (!r || cdata.len < 4)
        log_warn_return(LOG_EXIT_ZERO, "unknown cdb key: ", key) ;

    char pack[4] ;
    memcpy(pack, cdata.s, 4) ;
    u32_unpack_big(pack, field) ;

    return 1 ;
}

/** As resolve_get_key_u32, for a u64 key (needs at least eight bytes). */
int resolve_get_key_u64(const ocdb *c, const char *key, uint64_t *field)
{
    ocdb_data cdata ;

    int r = ocdb_find(c, &cdata, key, strlen(key)) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_ZERO, "search on cdb key: ", key) ;
    if (!r || cdata.len < 8)
        log_warn_return(LOG_EXIT_ZERO, "unknown cdb key: ", key) ;

    char pack[8] ;
    memcpy(pack, cdata.s, 8) ;
    u64_unpack_big(pack, field) ;

    return 1 ;
}
