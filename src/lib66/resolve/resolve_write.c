/*
 * resolve_write.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

int resolve_write(resolve_wrapper_t *wres, char const *dst, char const *name)
{
    log_flow() ;

    size_t dstlen = strlen(dst) ;
    size_t namelen = strlen(name) ;

    char tmp[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    auto_strings(tmp,dst,SS_RESOLVE,"/") ;

    if (!resolve_write_cdb(wres,tmp,name))
        return 0 ;

    return 1 ;
}
