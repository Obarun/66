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

#include <66/tree.h>

#include <sys/types.h>

#include <oblibs/types.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/ssexec.h>

int tree_isvalid(ssexec_t *info)
{
    int r ;

    char treename[info->base.len + SS_SYSTEM_LEN + 1 + info->treename.len + 1] ;
    auto_strings(treename, info->base.s, SS_SYSTEM, "/", info->treename.s) ;

    r = scan_mode(treename, S_IFDIR) ;
    if (r < 0)
        return -1 ;
    else if (!r)
        return 0 ;

    return 1 ;
}
