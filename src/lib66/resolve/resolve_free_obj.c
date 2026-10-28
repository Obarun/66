/*
 * resolve_free_obj.c
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
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/tree.h>
#include <66/service.h>

void resolve_free_obj(resolve_wrapper_t *wres)
{
    log_flow() ;

    RESOLVE_SET_SBWRES(wres) ;

    if (sbwres)
        strbuf_free(sbwres) ;
}
