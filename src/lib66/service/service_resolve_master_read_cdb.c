/*
 * service_resolve_master_read_cdb.c
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

#include <stdint.h>
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_master_read_cdb(cdb *c, resolve_service_master_t *tres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_SERVICE_MASTER, tres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    tres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* classic */
    resolve_find_cdb(&tmp,c,"classic") ;
    tres->classic = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* bundle */
    resolve_find_cdb(&tmp,c,"bundle") ;
    tres->bundle = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* longrun */
    resolve_find_cdb(&tmp,c,"longrun") ;
    tres->longrun = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* oneshot */
    resolve_find_cdb(&tmp,c,"oneshot") ;
    tres->oneshot = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* modules */
    resolve_find_cdb(&tmp,c,"module") ;
    tres->module = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* nclassic */
    x = resolve_find_cdb(&tmp,c,"nclassic") ;
    tres->nclassic = x ;

    /* nbundle */
    x = resolve_find_cdb(&tmp,c,"nbundle") ;
    tres->nbundle = x ;

    /* nlongrun */
    x = resolve_find_cdb(&tmp,c,"nlongrun") ;
    tres->nlongrun = x ;

    /* noneshot */
    x = resolve_find_cdb(&tmp,c,"noneshot") ;
    tres->noneshot = x ;

    /* nmodule */
    x = resolve_find_cdb(&tmp,c,"nmodule") ;
    tres->nmodule = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}
