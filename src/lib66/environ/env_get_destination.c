/*
 * env_get_destination.c
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

#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/constants.h>
#include <66/service.h>

int env_get_destination(strbuf *sa, resolve_service_t *res)
{
    log_flow() ;

    char *conf = res->sa.s + res->environ.envdir ;
    size_t conflen = strlen(conf) ;
    char sym[conflen + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(sym, conf, SS_SYM_VERSION) ;

    {
        char lnk[SS_MAX_PATH + 1] ;
        ssize_t lnklen = readlink(sym, lnk, sizeof(lnk) - 1) ;
        if (lnklen == -1)
            log_warnusys_return(LOG_EXIT_ZERO, "read link of: ", sym) ;
        if (!strbuf_copyb(sa, lnk, lnklen))
            log_warnusys_return(LOG_EXIT_ZERO, "strbuf") ;
    }

    if (!strbuf_terminate(sa))
        log_warnusys_return(LOG_EXIT_ZERO, "strbuf") ;

    return 1 ;
}
