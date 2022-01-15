/*
 * resolve_check.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>

int resolve_check(char const *src, char const *name)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    auto_strings(tmp, src, SS_RESOLVE, "/", name) ;

    r = scan_mode(tmp,S_IFREG) ;
    if (r <= 0)
        return 0 ;

    return 1 ;
}
