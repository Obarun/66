/*
 * env_prepare_for_write.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/environ.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/parse.h>

int env_prepare_for_write(stralloc *dst, stralloc *contents, resolve_service_t *res)
{
    log_flow() ;

    char *conf = res->sa.s + res->environ.envdir ;
    size_t conflen = strlen(conf) ;
    char sym[conflen + SS_SYM_VERSION_LEN + 1] ;

    auto_strings(sym, conf, SS_SYM_VERSION) ;

    if (!env_compute(contents, res))
        log_warnu_return(LOG_EXIT_ZERO, "compute environment") ;

    if (sareadlink(dst, sym) == -1)
        log_warnusys_return(LOG_EXIT_ZERO, "read link of: ", sym) ;

    if (!stralloc_0(dst))
        log_warnusys_return(LOG_EXIT_ZERO, "stralloc") ;

    return 1 ;
}
