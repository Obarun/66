/*
 * name_inlist.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/utils.h>

int name_inlist(char const *list, char const *name)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;

    if (!*list)
        return 0 ;

    if (!sbl_clean_string(&stk, list))
        return -1 ;

    return sbl_search(&stk, name) >= 0 ? 1 : 0 ;
}
