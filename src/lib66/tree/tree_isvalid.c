/*
 * tree_isvalid.c
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

#include <66/constants.h>

int tree_isvalid(char const *base, char const *treename)
{
    log_flow() ;

    int r ;
    size_t baselen = strlen(base), treelen = strlen(treename) ;
    char t[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + treelen + 1] ;
    auto_strings(t, base, SS_SYSTEM, SS_RESOLVE, "/", treename) ;

    r = scan_mode(t, S_IFREG) ;
    if (r < 0)
        return -1 ;
    else if (!r)
        return 0 ;

    return 1 ;
}
