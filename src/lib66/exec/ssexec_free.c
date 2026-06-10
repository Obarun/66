/*
 * ssexec_free.c
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

#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>

ssexec_t const ssexec_zero = SSEXEC_ZERO ;

void ssexec_free(ssexec_t *info)
{
    log_flow() ;

    strbuf_free(&info->base) ;
    strbuf_free(&info->live) ;
    strbuf_free(&info->scandir) ;
    strbuf_free(&info->treename) ;
    strbuf_free(&info->environment) ;
}


