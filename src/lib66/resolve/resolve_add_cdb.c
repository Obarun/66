/*
 * resolve_add_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/resolve.h>

int resolve_add_cdb(cdbmaker *c, char const *key, char const *str, uint32_t element, uint8_t check)
{
    char const *data = str + element ;
    size_t klen = strlen(key), dlen = strlen(data) ;

    if (check && !element) {
        data = "" ;
        dlen = 1 ;
    }

    if (!cdbmake_add(c,key,klen, data, dlen))
        log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

    return 1 ;
}
