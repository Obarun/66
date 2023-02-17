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

    // configuration
    if (!resolve_add_cdb(c, "name", str + sres->name) ||
    !resolve_add_cdb(c, "description", str + sres->description) ||
    !resolve_add_cdb(c, "version", str + sres->version) ||
    !resolve_add_cdb_uint(c, "type", sres->type) ||
    !resolve_add_cdb_uint(c, "notify", sres->notify) ||
    !resolve_add_cdb_uint(c, "maxdeath", sres->maxdeath) ||
    !resolve_add_cdb_uint(c, "earlier", sres->earlier) ||
    !resolve_add_cdb(c, "hiercopy", str + sres->hiercopy) ||
    !resolve_add_cdb(c, "intree", str + sres->intree) ||
    !resolve_add_cdb(c, "ownerstr", str + sres->ownerstr) ||
    !resolve_add_cdb_uint(c, "owner", sres->owner) ||
    !resolve_add_cdb(c, "treename", str + sres->treename) ||
    !resolve_add_cdb(c, "user", str + sres->user) ||
    !resolve_add_cdb(c, "inmodule", str + sres->inmodule) ||

    // path
    !resolve_add_cdb(c, "home", str + sres->path.home) ||
    !resolve_add_cdb(c, "frontend", str + sres->path.frontend) ||
    !resolve_add_cdb(c, "status", str + sres->path.status) ||

    // dependencies
    !resolve_add_cdb(c, "depends", str + sres->dependencies.depends) ||
    !resolve_add_cdb(c, "requiredby", str + sres->dependencies.requiredby) ||
    !resolve_add_cdb(c, "optsdeps", str + sres->dependencies.optsdeps) ||
    !resolve_add_cdb_uint(c, "ndepends", sres->dependencies.ndepends) ||
    !resolve_add_cdb_uint(c, "nrequiredby", sres->dependencies.nrequiredby) ||
    !resolve_add_cdb_uint(c, "noptsdeps", sres->dependencies.noptsdeps) ||

    // execute
    !resolve_add_cdb(c, "run", str + sres->execute.run.run) ||
    !resolve_add_cdb(c, "run_user", str + sres->execute.run.run_user) ||
    !resolve_add_cdb(c, "run_build", str + sres->execute.run.build) ||
    !resolve_add_cdb(c, "run_shebang", str + sres->execute.run.shebang) ||
    !resolve_add_cdb(c, "run_runas", str + sres->execute.run.runas) ||
    !resolve_add_cdb(c, "finish", str + sres->execute.finish.run) ||
    !resolve_add_cdb(c, "finish_user", str + sres->execute.finish.run_user) ||
    !resolve_add_cdb(c, "finish_build", str + sres->execute.finish.build) ||
    !resolve_add_cdb(c, "finish_shebang", str + sres->execute.finish.shebang) ||
    !resolve_add_cdb(c, "finish_runas", str + sres->execute.finish.runas) ||
    !resolve_add_cdb_uint(c, "timeoutkill", sres->execute.timeout.kill) ||
    !resolve_add_cdb_uint(c, "timeoutfinish", sres->execute.timeout.finish) ||
    !resolve_add_cdb_uint(c, "timeoutup", sres->execute.timeout.up) ||
    !resolve_add_cdb_uint(c, "timeoutdown", sres->execute.timeout.down) ||
    !resolve_add_cdb_uint(c, "down", sres->execute.down) ||
    !resolve_add_cdb_uint(c, "downsignal", sres->execute.downsignal) ||

    //live
    !resolve_add_cdb(c, "livedir", str + sres->live.livedir) ||
    !resolve_add_cdb(c, "scandir", str + sres->live.scandir) ||
    !resolve_add_cdb(c, "statedir", str + sres->live.statedir) ||
    !resolve_add_cdb(c, "eventdir", str + sres->live.eventdir) ||
    !resolve_add_cdb(c, "notifdir", str + sres->live.notifdir) ||
    !resolve_add_cdb(c, "supervisedir", str + sres->live.supervisedir) ||
    !resolve_add_cdb(c, "fdholderdir", str + sres->live.fdholderdir) ||
    !resolve_add_cdb(c, "oneshotddir", str + sres->live.oneshotddir) ||

    // logger
    !resolve_add_cdb(c, "logname", str + sres->logger.name) ||
    !resolve_add_cdb(c, "logdestination", str + sres->logger.destination) ||
    !resolve_add_cdb_uint(c, "logbackup", sres->logger.backup) ||
    !resolve_add_cdb_uint(c, "logmaxsize", sres->logger.maxsize) ||
    !resolve_add_cdb_uint(c, "logwant", sres->logger.want) ||
    !resolve_add_cdb_uint(c, "logtimestamp", sres->logger.timestamp) ||
    !resolve_add_cdb(c, "logrun", str + sres->logger.execute.run.run) ||
    !resolve_add_cdb(c, "logrun_user", str + sres->logger.execute.run.run_user) ||
    !resolve_add_cdb(c, "logrun_build", str + sres->logger.execute.run.build) ||
    !resolve_add_cdb(c, "logrun_shebang", str + sres->logger.execute.run.shebang) ||
    !resolve_add_cdb(c, "logrun_runas", str + sres->logger.execute.run.runas) ||
    !resolve_add_cdb_uint(c, "logtimeoutkill", sres->logger.execute.timeout.kill) ||
    !resolve_add_cdb_uint(c, "logtimeoutfinish", sres->logger.execute.timeout.finish) ||

    // environ
    !resolve_add_cdb(c, "env", str + sres->environ.env) ||
    !resolve_add_cdb(c, "envdir", str + sres->environ.envdir) ||
    !resolve_add_cdb_uint(c, "env_overwrite", sres->environ.env_overwrite) ||

    // regex
    !resolve_add_cdb(c, "configure", str + sres->regex.configure) ||
    !resolve_add_cdb(c, "directories", str + sres->regex.directories) ||
    !resolve_add_cdb(c, "files", str + sres->regex.files) ||
    !resolve_add_cdb(c, "infiles", str + sres->regex.infiles) ||
    !resolve_add_cdb_uint(c, "ndirectories", sres->regex.ndirectories) ||
    !resolve_add_cdb_uint(c, "nfiles", sres->regex.nfiles) ||
    !resolve_add_cdb_uint(c, "ninfiles", sres->regex.ninfiles)) return 0 ;

    return 1 ;
}
