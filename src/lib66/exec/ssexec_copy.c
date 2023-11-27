/*
 * ssexec_copy.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>

void ssexec_copy(ssexec_t *dest, ssexec_t *src)
{
    log_flow() ;

    auto_stra(&dest->base, src->base.s) ;
    auto_stra(&dest->live, src->live.s) ;
    auto_stra(&dest->scandir, src->scandir.s) ;
    auto_stra(&dest->treename, src->treename.s) ;

    dest->treeallow = src->treeallow ;
    dest->owner = src->owner ;
    auto_strings(dest->ownerstr, src->ownerstr) ;
    dest->ownerlen = src->ownerlen ;
    dest->timeout = src->timeout ;
    dest->prog = src->prog ;
    dest->help = src->help ;
    dest->usage = src->usage ;
    dest->opt_verbo = src->opt_verbo ;
    dest->opt_live = src->opt_live ;
    dest->opt_timeout = src->opt_timeout ;
    dest->opt_color = src->opt_color ;
    dest->skip_opt_tree = src->skip_opt_tree ;
}


