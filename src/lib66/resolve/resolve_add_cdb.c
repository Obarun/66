/*
 * resolve_add_cdb.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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

int resolve_add_cdb(cdbmaker *c, char const *key, char const *data)
{

    size_t klen = strlen(key) ;
    size_t dlen = strlen(data) ;

    if (!cdbmake_add(c,key,klen,dlen ? data : 0,dlen))
        log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

    return 1 ;
}
