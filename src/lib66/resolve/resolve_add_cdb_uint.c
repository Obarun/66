/*
 * resolve_add_cdb_uint.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <skalibs/cdbmake.h>
#include <skalibs/types.h>//uint##_pack

#include <66/resolve.h>

int resolve_add_cdb_uint(cdbmaker *c, char const *key, uint32_t data)
{

    char pack[4] ;
    size_t klen = strlen(key) ;

    uint32_pack_big(pack, data) ;
    if (!cdbmake_add(c,key,klen,pack,4))
        log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

    return 1 ;
}
