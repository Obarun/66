/*
 * set_treeinfo.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/utils.h>
#include <66/ssexec.h>
#include <66/tree.h>

int set_treeinfo(ssexec_t *info, strbuf *msg)
{
    log_flow() ;

    int r = tree_sethome(info) ;
    if (r == -1)
        return auto_strbuf(msg, "unable to set the tree name") ? 0 : -1 ;

    if (!r)
        return auto_strbuf(msg, "unable to parse seed file") ? 0 : -1 ;

    r = tree_get_permissions(info->base.s, info->treename.s) ;
    if (r < 0)
        return -1 ;
    if (!r)
        return auto_strbuf(msg, "You're not allowed to use the tree: ", info->treename.s) ? 0 : -1 ;

    return 1 ;
}
