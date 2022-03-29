/*
 * service_resolve_write_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_write_cdb(cdbmaker *c, resolve_service_t *sres)
{

    log_flow() ;

    char *str = sres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + sres->name) ||

    /* description */
    !resolve_add_cdb(c,"description",str + sres->description) ||

    /* version */
    !resolve_add_cdb(c,"version",str + sres->version) ||

    /* logger */
    !resolve_add_cdb(c,"logger",str + sres->logger) ||

    /* logreal */
    !resolve_add_cdb(c,"logreal",str + sres->logreal) ||

    /* logassoc */
    !resolve_add_cdb(c,"logassoc",str + sres->logassoc) ||

    /* dstlog */
    !resolve_add_cdb(c,"dstlog",str + sres->dstlog) ||

    /* depends */
    !resolve_add_cdb(c,"depends",str + sres->depends) ||

    /* deps */
    !resolve_add_cdb(c,"requiredby",str + sres->requiredby) ||

    /* optsdeps */
    !resolve_add_cdb(c,"optsdeps",str + sres->optsdeps) ||

    /* extdeps */
    !resolve_add_cdb(c,"extdeps",str + sres->extdeps) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str + sres->contents) ||

    /* src */
    !resolve_add_cdb(c,"src",str + sres->src) ||

    /* srconf */
    !resolve_add_cdb(c,"srconf",str + sres->srconf) ||

    /* live */
    !resolve_add_cdb(c,"live",str + sres->live) ||

    /* runat */
    !resolve_add_cdb(c,"runat",str + sres->runat) ||

    /* tree */
    !resolve_add_cdb(c,"tree",str + sres->tree) ||

    /* treename */
    !resolve_add_cdb(c,"treename",str + sres->treename) ||

    /* dstlog */
    !resolve_add_cdb(c,"dstlog",str + sres->dstlog) ||

    /* state */
    !resolve_add_cdb(c,"state",str + sres->state) ||

    /* exec_run */
    !resolve_add_cdb(c,"exec_run",str + sres->exec_run) ||

    /* exec_log_run */
    !resolve_add_cdb(c,"exec_log_run",str + sres->exec_log_run) ||

    /* real_exec_run */
    !resolve_add_cdb(c,"real_exec_run",str + sres->real_exec_run) ||

    /* real_exec_log_run */
    !resolve_add_cdb(c,"real_exec_log_run",str + sres->real_exec_log_run) ||

    /* exec_finish */
    !resolve_add_cdb(c,"exec_finish",str + sres->exec_finish) ||

    /* real_exec_finish */
    !resolve_add_cdb(c,"real_exec_finish",str + sres->real_exec_finish) ||

    /* type */
    !resolve_add_cdb_uint(c,"type",sres->type) ||

    /* ndepends */
    !resolve_add_cdb_uint(c,"ndepends",sres->ndepends) ||

    /* ndeps */
    !resolve_add_cdb_uint(c,"ndeps",sres->nrequiredby) ||

    /* noptsdeps */
    !resolve_add_cdb_uint(c,"noptsdeps",sres->noptsdeps) ||

    /* nextdeps */
    !resolve_add_cdb_uint(c,"nextdeps",sres->nextdeps) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",sres->ncontents) ||

    /* down */
    !resolve_add_cdb_uint(c,"down",sres->down) ||

    /* disen */
    !resolve_add_cdb_uint(c,"disen",sres->disen)) return 0 ;

    return 1 ;
}
