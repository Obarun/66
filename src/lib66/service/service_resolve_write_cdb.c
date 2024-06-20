/*
 * service_resolve_write_cdb.c
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

#include <oblibs/log.h>

#include <skalibs/cdbmake.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_write_cdb(cdbmaker *c, resolve_service_t *sres)
{

    log_flow() ;

    if (!cdbmake_add(c, "sa", 2, sres->sa.s, sres->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "name", sres->name) ||
        !resolve_add_cdb_uint(c, "description", sres->description) ||
        !resolve_add_cdb_uint(c, "version", sres->version) ||
        !resolve_add_cdb_uint(c, "type", sres->type) ||
        !resolve_add_cdb_uint(c, "notify", sres->notify) ||
        !resolve_add_cdb_uint(c, "maxdeath", sres->maxdeath) ||
        !resolve_add_cdb_uint(c, "earlier", sres->earlier) ||
        !resolve_add_cdb_uint(c, "hiercopy", sres->hiercopy) ||
        !resolve_add_cdb_uint(c, "intree", sres->intree) ||
        !resolve_add_cdb_uint(c, "ownerstr", sres->ownerstr) ||
        !resolve_add_cdb_uint(c, "owner", sres->owner) ||
        !resolve_add_cdb_uint(c, "treename", sres->treename) ||
        !resolve_add_cdb_uint(c, "user", sres->user) ||
        !resolve_add_cdb_uint(c, "inns", sres->inns) ||
        !resolve_add_cdb_uint(c, "enabled", sres->enabled) ||

        // path
        !resolve_add_cdb_uint(c, "home", sres->path.home) ||
        !resolve_add_cdb_uint(c, "frontend", sres->path.frontend) ||
        !resolve_add_cdb_uint(c, "src_servicedir", sres->path.servicedir) ||

        // dependencies
        !resolve_add_cdb_uint(c, "depends", sres->dependencies.depends) ||
        !resolve_add_cdb_uint(c, "requiredby", sres->dependencies.requiredby) ||
        !resolve_add_cdb_uint(c, "optsdeps", sres->dependencies.optsdeps) ||
        !resolve_add_cdb_uint(c, "contents", sres->dependencies.contents) ||
        !resolve_add_cdb_uint(c, "ndepends", sres->dependencies.ndepends) ||
        !resolve_add_cdb_uint(c, "nrequiredby", sres->dependencies.nrequiredby) ||
        !resolve_add_cdb_uint(c, "noptsdeps", sres->dependencies.noptsdeps) ||
        !resolve_add_cdb_uint(c, "ncontents", sres->dependencies.ncontents) ||

        // execute
        !resolve_add_cdb_uint(c, "run", sres->execute.run.run) ||
        !resolve_add_cdb_uint(c, "run_user", sres->execute.run.run_user) ||
        !resolve_add_cdb_uint(c, "run_build", sres->execute.run.build) ||
        !resolve_add_cdb_uint(c, "run_runas", sres->execute.run.runas) ||
        !resolve_add_cdb_uint(c, "finish", sres->execute.finish.run) ||
        !resolve_add_cdb_uint(c, "finish_user", sres->execute.finish.run_user) ||
        !resolve_add_cdb_uint(c, "finish_build", sres->execute.finish.build) ||
        !resolve_add_cdb_uint(c, "finish_runas", sres->execute.finish.runas) ||
        !resolve_add_cdb_uint(c, "timeoutstart", sres->execute.timeout.start) ||
        !resolve_add_cdb_uint(c, "timeoutstop", sres->execute.timeout.stop) ||
        !resolve_add_cdb_uint(c, "down", sres->execute.down) ||
        !resolve_add_cdb_uint(c, "downsignal", sres->execute.downsignal) ||

        //live
        !resolve_add_cdb_uint(c, "livedir", sres->live.livedir) ||
        !resolve_add_cdb_uint(c, "status", sres->live.status) ||
        !resolve_add_cdb_uint(c, "live_servicedir", sres->live.servicedir) ||
        !resolve_add_cdb_uint(c, "scandir", sres->live.scandir) ||
        !resolve_add_cdb_uint(c, "statedir", sres->live.statedir) ||
        !resolve_add_cdb_uint(c, "eventdir", sres->live.eventdir) ||
        !resolve_add_cdb_uint(c, "notifdir", sres->live.notifdir) ||
        !resolve_add_cdb_uint(c, "supervisedir", sres->live.supervisedir) ||
        !resolve_add_cdb_uint(c, "fdholderdir", sres->live.fdholderdir) ||
        !resolve_add_cdb_uint(c, "oneshotddir", sres->live.oneshotddir) ||

        // logger
        !resolve_add_cdb_uint(c, "logname", sres->logger.name) ||
        !resolve_add_cdb_uint(c, "logdestination", sres->logger.destination) ||
        !resolve_add_cdb_uint(c, "logbackup", sres->logger.backup) ||
        !resolve_add_cdb_uint(c, "logmaxsize", sres->logger.maxsize) ||
        !resolve_add_cdb_uint(c, "logwant", sres->logger.want) ||
        !resolve_add_cdb_uint(c, "logtimestamp", sres->logger.timestamp) ||
        !resolve_add_cdb_uint(c, "logrun", sres->logger.execute.run.run) ||
        !resolve_add_cdb_uint(c, "logrun_user", sres->logger.execute.run.run_user) ||
        !resolve_add_cdb_uint(c, "logrun_build", sres->logger.execute.run.build) ||
        !resolve_add_cdb_uint(c, "logrun_runas", sres->logger.execute.run.runas) ||
        !resolve_add_cdb_uint(c, "logtimeoutstart", sres->logger.execute.timeout.start) ||
        !resolve_add_cdb_uint(c, "logtimeoutstop", sres->logger.execute.timeout.stop) ||

        // environ
        !resolve_add_cdb_uint(c, "env", sres->environ.env) ||
        !resolve_add_cdb_uint(c, "envdir", sres->environ.envdir) ||
        !resolve_add_cdb_uint(c, "env_overwrite", sres->environ.env_overwrite) ||

        // regex
        !resolve_add_cdb_uint(c, "configure", sres->regex.configure) ||
        !resolve_add_cdb_uint(c, "directories", sres->regex.directories) ||
        !resolve_add_cdb_uint(c, "files", sres->regex.files) ||
        !resolve_add_cdb_uint(c, "infiles", sres->regex.infiles) ||
        !resolve_add_cdb_uint(c, "ndirectories", sres->regex.ndirectories) ||
        !resolve_add_cdb_uint(c, "nfiles", sres->regex.nfiles) ||
        !resolve_add_cdb_uint(c, "ninfiles", sres->regex.ninfiles))
            return 0 ;

    return 1 ;
}
