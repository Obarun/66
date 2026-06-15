/*
 * set_livedir.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/utils.h>

int set_livedir(strbuf *live)
{
    log_flow() ;

    if (live->len) {

        if (live->s[0] != '/')
            return -1 ;

        if (live->s[live->len - 1] != '/')
            if (!auto_strbuf(live,"/")) log_warnsys_return(LOG_EXIT_ZERO,"strbuf") ;

    } else if (!auto_strbuf(live,SS_LIVE))
        log_warnsys_return(LOG_EXIT_ZERO,"strbuf") ;

    return 1 ;
}
