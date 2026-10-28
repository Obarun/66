/*
 * env_append_version.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>

int env_append_version(strbuf *saversion, char const *svconf, char const *version)
{
    log_flow() ;

    int r ;

    char stk[strlen(version) + 1] ;

    auto_strings(stk, version) ;

    if (!auto_strbuf(saversion,svconf,"/",stk))
        return -1 ;

    // a version that is no directory is no version
    r = scan_mode(saversion->s,FLAGS_DIR) ;

    if (!r && errno != ENOENT)
        return -1 ;

    return r > 0 ? 1 : 0 ;
}
