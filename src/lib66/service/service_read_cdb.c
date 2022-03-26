/*
 * service_read_cdb.c
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

int service_read_cdb(cdb *c, resolve_service_t *res)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    res->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* description */
    resolve_find_cdb(&tmp,c,"description") ;
    res->description = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* version */
    resolve_find_cdb(&tmp,c,"version") ;
    res->version = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logger */
    resolve_find_cdb(&tmp,c,"logger") ;
    res->logger = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logreal */
    resolve_find_cdb(&tmp,c,"logreal") ;
    res->logreal = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logassoc */
    resolve_find_cdb(&tmp,c,"logassoc") ;
    res->logassoc = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* dstlog */
    resolve_find_cdb(&tmp,c,"dstlog") ;
    res->dstlog = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* depends */
    resolve_find_cdb(&tmp,c,"depends") ;
    res->depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* requiredby */
    resolve_find_cdb(&tmp,c,"requiredby") ;
    res->requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* optsdeps */
    resolve_find_cdb(&tmp,c,"optsdeps") ;
    res->optsdeps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* extdeps */
    resolve_find_cdb(&tmp,c,"extdeps") ;
    res->extdeps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    res->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* src */
    resolve_find_cdb(&tmp,c,"src") ;
    res->src = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* srconf */
    resolve_find_cdb(&tmp,c,"srconf") ;
    res->srconf = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* live */
    resolve_find_cdb(&tmp,c,"live") ;
    res->live = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* runat */
    resolve_find_cdb(&tmp,c,"runat") ;
    res->runat = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* tree */
    resolve_find_cdb(&tmp,c,"tree") ;
    res->tree = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* treename */
    resolve_find_cdb(&tmp,c,"treename") ;
    res->treename = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* state */
    resolve_find_cdb(&tmp,c,"state") ;
    res->state = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_run */
    resolve_find_cdb(&tmp,c,"exec_run") ;
    res->exec_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_log_run */
    resolve_find_cdb(&tmp,c,"exec_log_run") ;
    res->exec_log_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_run */
    resolve_find_cdb(&tmp,c,"real_exec_run") ;
    res->real_exec_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_log_run */
    resolve_find_cdb(&tmp,c,"real_exec_log_run") ;
    res->real_exec_log_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_finish */
    resolve_find_cdb(&tmp,c,"exec_finish") ;
    res->exec_finish = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_finish */
    resolve_find_cdb(&tmp,c,"real_exec_finish") ;
    res->real_exec_finish = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* type */
    x = resolve_find_cdb(&tmp,c,"type") ;
    res->type = x ;

    /* ndepends */
    x = resolve_find_cdb(&tmp,c,"ndepends") ;
    res->ndepends = x ;

    /* nrequiredby */
    x = resolve_find_cdb(&tmp,c,"nrequiredby") ;
    res->nrequiredby = x ;

    /* noptsdeps */
    x = resolve_find_cdb(&tmp,c,"noptsdeps") ;
    res->noptsdeps = x ;

    /* nextdeps */
    x = resolve_find_cdb(&tmp,c,"nextdeps") ;
    res->nextdeps = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    res->ncontents = x ;

    /* down */
    x = resolve_find_cdb(&tmp,c,"down") ;
    res->down = x ;

    /* disen */
    x = resolve_find_cdb(&tmp,c,"disen") ;
    res->disen = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}
