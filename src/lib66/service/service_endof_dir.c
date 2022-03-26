/*
 * service_endof_dir.c
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
#include <oblibs/types.h>

#include <66/service.h>

int service_endof_dir(char const *dir, char const *name)
{
    log_flow() ;

    size_t dirlen = strlen(dir) ;
    size_t namelen = strlen(name) ;
    char t[dirlen + 1 + namelen + 1] ;
    auto_strings(t, dir, "/", name) ;

    int r = scan_mode(t,S_IFREG) ;

    if (!ob_basename(t,dir))
        return -1 ;

    if (!strcmp(t,name) && (r > 0))
        return 1 ;

    return 0 ;
}
