/*
 * service_resolve_sanitize.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

void service_resolve_sanitize(resolve_service_t *res)
{
    log_flow() ;

    char stk[res->sa.len + 1] ;

    memcpy(stk, res->sa.s, res->sa.len) ;
    stk[res->sa.len] = 0 ;

    res->sa.len = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    resolve_init(wres) ;

    // configuration
    res->name = resolve_add_string(wres, stk + res->name) ;
    res->description = res->description ? resolve_add_string(wres, stk + res->description) : 0 ;
    res->version = res->version ? resolve_add_string(wres, stk + res->version) : 0 ;
    res->hiercopy = res->hiercopy ? resolve_add_string(wres, stk + res->hiercopy) : 0 ;
    res->intree = res->intree ? resolve_add_string(wres, stk + res->intree) : 0 ;
    res->ownerstr = res->ownerstr ? resolve_add_string(wres, stk + res->ownerstr) : 0 ;
    res->treename = res->treename ? resolve_add_string(wres, stk + res->treename) : 0 ;
    res->user = res->user ? resolve_add_string(wres, stk + res->user) : 0 ;
    res->inns = res->inns ? resolve_add_string(wres, stk + res->inns) : 0 ;

    // path
    res->path.home = res->path.home ? resolve_add_string(wres, stk + res->path.home) : 0 ;
    res->path.frontend = res->path.frontend ? resolve_add_string(wres, stk + res->path.frontend) : 0 ;
    res->path.servicedir = res->path.servicedir ? resolve_add_string(wres, stk + res->path.servicedir) : 0 ;

    // dependencies
    res->dependencies.depends = res->dependencies.depends ? resolve_add_string(wres, stk + res->dependencies.depends) : 0 ;
    res->dependencies.requiredby = res->dependencies.requiredby ? resolve_add_string(wres, stk + res->dependencies.requiredby) : 0 ;
    res->dependencies.optsdeps = res->dependencies.optsdeps ? resolve_add_string(wres, stk + res->dependencies.optsdeps) : 0 ;
    res->dependencies.contents = res->dependencies.contents ? resolve_add_string(wres, stk + res->dependencies.contents) : 0 ;

    // execute
    res->execute.run.run = res->execute.run.run ? resolve_add_string(wres, stk + res->execute.run.run) : 0 ;
    res->execute.run.run_user = res->execute.run.run_user ? resolve_add_string(wres, stk + res->execute.run.run_user) : 0 ;
    res->execute.run.build = res->execute.run.build ? resolve_add_string(wres, stk + res->execute.run.build) : 0 ;
    res->execute.run.runas = res->execute.run.runas ? resolve_add_string(wres, stk + res->execute.run.runas) : 0 ;
    res->execute.finish.run = res->execute.finish.run ? resolve_add_string(wres, stk + res->execute.finish.run) : 0 ;
    res->execute.finish.run_user = res->execute.finish.run_user ? resolve_add_string(wres, stk + res->execute.finish.run_user) : 0 ;
    res->execute.finish.build = res->execute.finish.build ? resolve_add_string(wres, stk + res->execute.finish.build) : 0 ;
    res->execute.finish.runas = res->execute.finish.runas ? resolve_add_string(wres, stk + res->execute.finish.runas) : 0 ;

    // live
    res->live.livedir = res->live.livedir ? resolve_add_string(wres, stk + res->live.livedir) : 0 ;
    res->live.status = res->live.status ? resolve_add_string(wres, stk + res->live.status) : 0 ;
    res->live.servicedir = res->live.servicedir ? resolve_add_string(wres, stk + res->live.servicedir) : 0 ;
    res->live.scandir = res->live.scandir ? resolve_add_string(wres, stk + res->live.scandir) : 0 ;
    res->live.statedir = res->live.statedir ? resolve_add_string(wres, stk + res->live.statedir) : 0 ;
    res->live.eventdir = res->live.eventdir ? resolve_add_string(wres, stk + res->live.eventdir) : 0 ;
    res->live.notifdir = res->live.notifdir ? resolve_add_string(wres, stk + res->live.notifdir) : 0 ;
    res->live.supervisedir = res->live.supervisedir ? resolve_add_string(wres, stk + res->live.supervisedir) : 0 ;
    res->live.fdholderdir = res->live.fdholderdir ? resolve_add_string(wres, stk + res->live.fdholderdir) : 0 ;
    res->live.oneshotddir = res->live.oneshotddir ? resolve_add_string(wres, stk + res->live.oneshotddir) : 0 ;

    // logger
    res->logger.name = res->logger.name ? resolve_add_string(wres, stk + res->logger.name) : 0 ;
    res->logger.execute.run.run = res->logger.execute.run.run ? resolve_add_string(wres, stk + res->logger.execute.run.run) : 0 ;
    res->logger.execute.run.run_user = res->logger.execute.run.run_user ? resolve_add_string(wres, stk + res->logger.execute.run.run_user) : 0 ;
    res->logger.execute.run.build = res->logger.execute.run.build ? resolve_add_string(wres, stk + res->logger.execute.run.build) : 0 ;
    res->logger.execute.run.runas = res->logger.execute.run.runas ? resolve_add_string(wres, stk + res->logger.execute.run.runas) : 0 ;

    // environment
    res->environ.env = res->environ.env ? resolve_add_string(wres, stk + res->environ.env) : 0 ;
    res->environ.envdir = res->environ.envdir ? resolve_add_string(wres, stk + res->environ.envdir) : 0 ;

    // regex
    res->regex.configure = res->regex.configure ? resolve_add_string(wres, stk + res->regex.configure) : 0 ;
    res->regex.directories = res->regex.directories ? resolve_add_string(wres, stk + res->regex.directories) : 0 ;
    res->regex.files = res->regex.files ? resolve_add_string(wres, stk + res->regex.files) : 0 ;
    res->regex.infiles = res->regex.infiles ? resolve_add_string(wres, stk + res->regex.infiles) : 0 ;

    // IO
    res->io.fdin.destination = res->io.fdin.destination ? resolve_add_string(wres, stk + res->io.fdin.destination) : 0 ;
    res->io.fdout.destination = res->io.fdout.destination ? resolve_add_string(wres, stk + res->io.fdout.destination) : 0 ;
    res->io.fderr.destination = res->io.fderr.destination ? resolve_add_string(wres, stk + res->io.fderr.destination) : 0 ;

    res->rversion = res->rversion ? resolve_add_string(wres, stk + res->rversion) : 0 ;

    free(wres) ;
}
