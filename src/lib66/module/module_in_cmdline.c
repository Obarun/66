/*
 * module_in_cmdline.c
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

#include <stdlib.h>

#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/utils.h>

int module_in_cmdline(genalloc *gares, resolve_service_t *res, char const *dir)
{
    log_flow() ;

    int e = 0 ;
/*    stralloc tmp = STRALLOC_ZERO ;
    size_t pos = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!resolve_append(gares,wres)) goto err ;

    if (res->dependencies.depends)
    {
        if (!sastr_clean_string(&tmp,res->sa.s + res->dependencies.depends))
            goto err ;
    }
    for (; pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
    {
        char *name = tmp.s + pos ;
        if (!resolve_check(dir,name)) goto err ;
        if (!resolve_read(wres,dir,name)) goto err ;
        if (res->type == TYPE_CLASSIC)
            if (resolve_search(gares,name, DATA_SERVICE) < 0)
                if (!resolve_append(gares,wres)) goto err ;
    }

    e = 1 ;

    err:
        free(wres) ;
        stralloc_free(&tmp) ;
  */      return e ;
}
