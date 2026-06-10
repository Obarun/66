/*
 * instance_create.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

int instance_create(strbuf *sasv,char const *svname, char const *regex, int len)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1 ;

    _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;

    if (!auto_strbuf(&tmp,sasv->s)) return 0 ;

    copy = svname + tlen ;

    if (!sbl_replace_nline(&tmp,regex,copy))
        log_warnu_return(LOG_EXIT_ZERO, "replace instance character for service: ",svname) ;

    sasv->len = 0 ;

    return auto_strbuf(sasv, tmp.s) ;
}
