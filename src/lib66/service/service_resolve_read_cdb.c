/*
 * service_resolve_read_cdb.c
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

#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/cdb.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_read_cdb(cdb *c, resolve_service_t *res)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (resolve_get_sa(&res->sa,c) <= 0 || !res->sa.len)
        return 0 ;

    /* configuration */
    if (!resolve_get_key(c, "name", &res->name) ||
        !resolve_get_key(c, "description", &res->description) ||
        !resolve_get_key(c, "version", &res->version) ||
        !resolve_get_key(c, "type", &res->type) ||
        !resolve_get_key(c, "notify", &res->notify) ||
        !resolve_get_key(c, "maxdeath", &res->maxdeath) ||
        !resolve_get_key(c, "earlier", &res->earlier) ||
        !resolve_get_key(c, "hiercopy", &res->hiercopy) ||
        !resolve_get_key(c, "intree", &res->intree) ||
        !resolve_get_key(c, "ownerstr", &res->ownerstr) ||
        !resolve_get_key(c, "owner", &res->owner) ||
        !resolve_get_key(c, "treename", &res->treename) ||
        !resolve_get_key(c, "user", &res->user) ||
        !resolve_get_key(c, "inns", &res->inns) ||
        !resolve_get_key(c, "enabled", &res->enabled) ||

    /* path configuration */
        !resolve_get_key(c, "home", &res->path.home) ||
        !resolve_get_key(c, "frontend", &res->path.frontend) ||
        !resolve_get_key(c, "src_servicedir", &res->path.servicedir) ||

    /* dependencies */
        !resolve_get_key(c, "depends", &res->dependencies.depends) ||
        !resolve_get_key(c, "requiredby", &res->dependencies.requiredby) ||
        !resolve_get_key(c, "optsdeps", &res->dependencies.optsdeps) ||
        !resolve_get_key(c, "contents", &res->dependencies.contents) ||
        !resolve_get_key(c, "ndepends", &res->dependencies.ndepends) ||
        !resolve_get_key(c, "nrequiredby", &res->dependencies.nrequiredby) ||
        !resolve_get_key(c, "noptsdeps", &res->dependencies.noptsdeps) ||
        !resolve_get_key(c, "ncontents", &res->dependencies.ncontents) ||

    /* execute */
        !resolve_get_key(c, "run", &res->execute.run.run) ||
        !resolve_get_key(c, "run_user", &res->execute.run.run_user) ||
        !resolve_get_key(c, "run_build", &res->execute.run.build) ||
        !resolve_get_key(c, "run_runas", &res->execute.run.runas) ||
        !resolve_get_key(c, "finish", &res->execute.finish.run) ||
        !resolve_get_key(c, "finish_user", &res->execute.finish.run_user) ||
        !resolve_get_key(c, "finish_build", &res->execute.finish.build) ||
        !resolve_get_key(c, "finish_runas", &res->execute.finish.runas) ||
        !resolve_get_key(c, "timeoutstart", &res->execute.timeout.start) ||
        !resolve_get_key(c, "timeoutstop", &res->execute.timeout.stop) ||
        !resolve_get_key(c, "down", &res->execute.down) ||
        !resolve_get_key(c, "downsignal", &res->execute.downsignal) ||

    /* live */
        !resolve_get_key(c, "livedir", &res->live.livedir) ||
        !resolve_get_key(c, "status", &res->live.status) ||
        !resolve_get_key(c, "live_servicedir", &res->live.servicedir) ||
        !resolve_get_key(c, "scandir", &res->live.scandir) ||
        !resolve_get_key(c, "statedir", &res->live.statedir) ||
        !resolve_get_key(c, "eventdir", &res->live.eventdir) ||
        !resolve_get_key(c, "notifdir", &res->live.notifdir) ||
        !resolve_get_key(c, "supervisedir", &res->live.supervisedir) ||
        !resolve_get_key(c, "fdholderdir", &res->live.fdholderdir) ||
        !resolve_get_key(c, "oneshotddir", &res->live.oneshotddir) ||

    /* logger */
        !resolve_get_key(c, "logname", &res->logger.name) ||
        !resolve_get_key(c, "logdestination", &res->logger.destination) ||
        !resolve_get_key(c, "logbackup", &res->logger.backup) ||
        !resolve_get_key(c, "logmaxsize", &res->logger.maxsize) ||
        !resolve_get_key(c, "logtimestamp", &res->logger.timestamp) ||
        !resolve_get_key(c, "logwant", &res->logger.want) ||
        !resolve_get_key(c, "logrun", &res->logger.execute.run.run) ||
        !resolve_get_key(c, "logrun_user", &res->logger.execute.run.run_user) ||
        !resolve_get_key(c, "logrun_build", &res->logger.execute.run.build) ||
        !resolve_get_key(c, "logrun_runas", &res->logger.execute.run.runas) ||
        !resolve_get_key(c, "logtimeoutstart", &res->logger.timeout.start) ||
        !resolve_get_key(c, "logtimeoutstop", &res->logger.timeout.stop) ||

    /* environment */
        !resolve_get_key(c, "env", &res->environ.env) ||
        !resolve_get_key(c, "envdir", &res->environ.envdir) ||
        !resolve_get_key(c, "env_overwrite", &res->environ.env_overwrite) ||

    /* regex */
        !resolve_get_key(c, "configure", &res->regex.configure) ||
        !resolve_get_key(c, "directories", &res->regex.directories) ||
        !resolve_get_key(c, "files", &res->regex.files) ||
        !resolve_get_key(c, "infiles", &res->regex.infiles) ||
        !resolve_get_key(c, "ndirectories", &res->regex.ndirectories) ||
        !resolve_get_key(c, "nfiles", &res->regex.nfiles) ||
        !resolve_get_key(c, "ninfiles", &res->regex.ninfiles)) {
            free(wres) ;
            return 0 ;
    }

    free(wres) ;

    return 1 ;
}
