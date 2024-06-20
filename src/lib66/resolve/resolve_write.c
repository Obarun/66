/*
 * resolve_write.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>

int resolve_write(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    size_t baselen = strlen(base) ;

    char path[baselen + SS_RESOLVE_LEN + 2] ;
    auto_strings(path, base, SS_RESOLVE, "/") ;

    return resolve_write_cdb(wres, path, name) ;

}
