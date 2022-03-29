/*
 * service_resolve_master_write_cdb.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

#include <skalibs/cdbmake.h>

#include <66/service.h>
#include <66/resolve.h>

int service_resolve_master_write_cdb(cdbmaker *c, resolve_service_master_t *mres)
{
    log_flow() ;

    char *str = mres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + mres->name) ||

    /* classic */
    !resolve_add_cdb(c,"classic",str + mres->classic) ||

    /* bundle */
    !resolve_add_cdb(c,"bundle",str + mres->bundle) ||

    /* longrun */
    !resolve_add_cdb(c,"longrun",str + mres->longrun) ||

    /* oneshot */
    !resolve_add_cdb(c,"oneshot",str + mres->oneshot) ||

    /* module */
    !resolve_add_cdb(c,"module",str + mres->module) ||

    /* nclassic */
    !resolve_add_cdb_uint(c,"nclassic",mres->nclassic) ||

    /* nbundle */
    !resolve_add_cdb_uint(c,"nbundle",mres->nbundle) ||

    /* nlongrun */
    !resolve_add_cdb_uint(c,"nlongrun",mres->nlongrun) ||

    /* noneshot */
    !resolve_add_cdb_uint(c,"noneshot",mres->noneshot) ||

    /* nmodule */
    !resolve_add_cdb_uint(c,"nmodule",mres->nmodule)) return 0 ;

    return 1 ;
}
