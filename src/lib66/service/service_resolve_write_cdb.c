/*
 * service_resolve_write_cdb.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/cdb.h>

#include <66/resolve.h>
#include <66/service.h>

static void add_version(resolve_service_t *res)
{
    log_flow() ;
    log_trace("resolve file version for: ", res->sa.s + res->name, " set to: ", SS_VERSION) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    res->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;
}

int service_resolve_write_cdb(ocdbmaker *c, resolve_service_t *res)
{

    log_flow() ;

    add_version(res) ;

    if (!ocdb_make_add(c, "sa", 2, res->sa.s, res->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "rversion", res->rversion) ||
        !resolve_add_cdb_uint(c, "name", res->name) ||
        !resolve_add_cdb_uint(c, "description", res->description) ||
        !resolve_add_cdb_uint(c, "version", res->version) ||
        !resolve_add_cdb_uint(c, "type", res->type) ||
        !resolve_add_cdb_uint(c, "notify", res->notify) ||
        !resolve_add_cdb_uint(c, "maxdeath", res->maxdeath) ||
        !resolve_add_cdb_uint(c, "maxdeathtime", res->maxdeathtime) ||
        !resolve_add_cdb_uint(c, "earlier", res->earlier) ||
        !resolve_add_cdb_uint(c, "copyfrom", res->copyfrom) ||
        !resolve_add_cdb_uint(c, "intree", res->intree) ||
        !resolve_add_cdb_uint(c, "ownerstr", res->ownerstr) ||
        !resolve_add_cdb_uint(c, "owner", res->owner) ||
        !resolve_add_cdb_uint(c, "treename", res->treename) ||
        !resolve_add_cdb_uint(c, "user", res->user) ||
        !resolve_add_cdb_uint(c, "inns", res->inns) ||
        !resolve_add_cdb_uint(c, "enabled", res->enabled) ||
        !resolve_add_cdb_uint(c, "islog", res->islog) ||
        !resolve_add_cdb_uint(c, "has_limit", res->has_limit) ||
        !resolve_add_cdb_uint(c, "has_environ", res->has_environ) ||

        // path
        !resolve_add_cdb_uint(c, "home", res->path.home) ||
        !resolve_add_cdb_uint(c, "frontend", res->path.frontend) ||
        !resolve_add_cdb_uint(c, "src_servicedir", res->path.servicedir) ||

        // dependencies
        !resolve_add_cdb_uint(c, "depends", res->dependencies.depends) ||
        !resolve_add_cdb_uint(c, "requiredby", res->dependencies.requiredby) ||
        !resolve_add_cdb_uint(c, "optsdeps", res->dependencies.optsdeps) ||
        !resolve_add_cdb_uint(c, "contents", res->dependencies.contents) ||
        !resolve_add_cdb_uint(c, "provide", res->dependencies.provide) ||
        !resolve_add_cdb_uint(c, "conflict", res->dependencies.conflict) ||
        !resolve_add_cdb_uint(c, "ndepends", res->dependencies.ndepends) ||
        !resolve_add_cdb_uint(c, "nrequiredby", res->dependencies.nrequiredby) ||
        !resolve_add_cdb_uint(c, "noptsdeps", res->dependencies.noptsdeps) ||
        !resolve_add_cdb_uint(c, "ncontents", res->dependencies.ncontents) ||
        !resolve_add_cdb_uint(c, "nprovide", res->dependencies.nprovide) ||
        !resolve_add_cdb_uint(c, "nconflict", res->dependencies.nconflict) ||

        // execute
        !resolve_add_cdb_uint(c, "run", res->execute.run.run) ||
        !resolve_add_cdb_uint(c, "run_user", res->execute.run.run_user) ||
        !resolve_add_cdb_uint(c, "run_build", res->execute.run.build) ||
        !resolve_add_cdb_uint(c, "run_runas", res->execute.run.runas) ||
        !resolve_add_cdb_uint(c, "finish", res->execute.finish.run) ||
        !resolve_add_cdb_uint(c, "finish_user", res->execute.finish.run_user) ||
        !resolve_add_cdb_uint(c, "finish_build", res->execute.finish.build) ||
        !resolve_add_cdb_uint(c, "finish_runas", res->execute.finish.runas) ||
        !resolve_add_cdb_uint(c, "timeoutstart", res->execute.timeout.start) ||
        !resolve_add_cdb_uint(c, "timeoutstop", res->execute.timeout.stop) ||
        !resolve_add_cdb_uint(c, "down", res->execute.down) ||
        !resolve_add_cdb_uint(c, "downsignal", res->execute.downsignal) ||
        !resolve_add_cdb_uint(c, "blockprivileges", res->execute.blockprivileges) ||
        !resolve_add_cdb_uint(c, "umask", res->execute.umask) ||
        !resolve_add_cdb_uint(c, "want_umask", res->execute.want_umask) ||
        !resolve_add_cdb_uint(c, "nice", res->execute.nice) ||
        !resolve_add_cdb_uint(c, "want_nice", res->execute.want_nice) ||
        !resolve_add_cdb_uint(c, "chdir", res->execute.chdir) ||
        !resolve_add_cdb_uint(c, "capsbound", res->execute.capsbound) ||
        !resolve_add_cdb_uint(c, "capsambient", res->execute.capsambient) ||
        !resolve_add_cdb_uint(c, "ncapsbound", res->execute.ncapsbound) ||
        !resolve_add_cdb_uint(c, "ncapsambient", res->execute.ncapsambient) ||

        //live
        !resolve_add_cdb_uint(c, "livedir", res->live.livedir) ||
        !resolve_add_cdb_uint(c, "status", res->live.status) ||
        !resolve_add_cdb_uint(c, "live_servicedir", res->live.servicedir) ||
        !resolve_add_cdb_uint(c, "scandir", res->live.scandir) ||
        !resolve_add_cdb_uint(c, "statedir", res->live.statedir) ||
        !resolve_add_cdb_uint(c, "eventdir", res->live.eventdir) ||
        !resolve_add_cdb_uint(c, "notifdir", res->live.notifdir) ||
        !resolve_add_cdb_uint(c, "supervisedir", res->live.supervisedir) ||
        !resolve_add_cdb_uint(c, "fdholderdir", res->live.fdholderdir) ||
        !resolve_add_cdb_uint(c, "oneshotddir", res->live.oneshotddir) ||

        // logger
        !resolve_add_cdb_uint(c, "logname", res->logger.name) ||
        !resolve_add_cdb_uint(c, "logbackup", res->logger.backup) ||
        !resolve_add_cdb_uint(c, "logmaxsize", res->logger.maxsize) ||
        !resolve_add_cdb_uint(c, "logwant", res->logger.want) ||
        !resolve_add_cdb_uint(c, "logtimestamp", res->logger.timestamp) ||
        !resolve_add_cdb_uint(c, "logrun", res->logger.execute.run.run) ||
        !resolve_add_cdb_uint(c, "logrun_user", res->logger.execute.run.run_user) ||
        !resolve_add_cdb_uint(c, "logrun_build", res->logger.execute.run.build) ||
        !resolve_add_cdb_uint(c, "logrun_runas", res->logger.execute.run.runas) ||
        !resolve_add_cdb_uint(c, "logtimeoutstart", res->logger.execute.timeout.start) ||
        !resolve_add_cdb_uint(c, "logtimeoutstop", res->logger.execute.timeout.stop) ||

        // regex
        !resolve_add_cdb_uint(c, "configure", res->regex.configure) ||
        !resolve_add_cdb_uint(c, "directories", res->regex.directories) ||
        !resolve_add_cdb_uint(c, "files", res->regex.files) ||
        !resolve_add_cdb_uint(c, "infiles", res->regex.infiles) ||
        !resolve_add_cdb_uint(c, "ndirectories", res->regex.ndirectories) ||
        !resolve_add_cdb_uint(c, "nfiles", res->regex.nfiles) ||
        !resolve_add_cdb_uint(c, "ninfiles", res->regex.ninfiles) ||

        // IO
        !resolve_add_cdb_uint(c, "stdintype", res->io.fdin.type) ||
        !resolve_add_cdb_uint(c, "stdindest", res->io.fdin.destination) ||
        !resolve_add_cdb_uint(c, "stdouttype", res->io.fdout.type) ||
        !resolve_add_cdb_uint(c, "stdoutdest", res->io.fdout.destination) ||
        !resolve_add_cdb_uint(c, "stderrtype", res->io.fderr.type) ||
        !resolve_add_cdb_uint(c, "stderrdest", res->io.fderr.destination))
            return 0 ;

    return 1 ;
}

int service_resolve_write_addon_limit_cdb(ocdbmaker *c, resolve_service_addon_limit_t *l)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_LIMIT, l) ;
    l->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;

    if (!ocdb_make_add(c, "sa", 2, l->sa.s, l->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "rversion", l->rversion) ||
        !resolve_add_cdb_uint64(c, "limitas", l->limitas) ||
        !resolve_add_cdb_uint64(c, "limitcore", l->limitcore) ||
        !resolve_add_cdb_uint64(c, "limitcpu", l->limitcpu) ||
        !resolve_add_cdb_uint64(c, "limitdata", l->limitdata) ||
        !resolve_add_cdb_uint64(c, "limitfsize", l->limitfsize) ||
        !resolve_add_cdb_uint64(c, "limitlocks", l->limitlocks) ||
        !resolve_add_cdb_uint64(c, "limitmemlock", l->limitmemlock) ||
        !resolve_add_cdb_uint64(c, "limitmsgqueue", l->limitmsgqueue) ||
        !resolve_add_cdb_uint64(c, "limitnice", l->limitnice) ||
        !resolve_add_cdb_uint64(c, "limitnofile", l->limitnofile) ||
        !resolve_add_cdb_uint64(c, "limitnproc", l->limitnproc) ||
        !resolve_add_cdb_uint64(c, "limitrtprio", l->limitrtprio) ||
        !resolve_add_cdb_uint64(c, "limitrttime", l->limitrttime) ||
        !resolve_add_cdb_uint64(c, "limitsigpending", l->limitsigpending) ||
        !resolve_add_cdb_uint64(c, "limitstack", l->limitstack))
            return 0 ;

    return 1 ;
}

int service_resolve_write_addon_environ_cdb(ocdbmaker *c, resolve_service_addon_environ_t *e)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;
    e->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;

    if (!ocdb_make_add(c, "sa", 2, e->sa.s, e->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "rversion", e->rversion) ||
        !resolve_add_cdb_uint(c, "env", e->env) ||
        !resolve_add_cdb_uint(c, "envdir", e->envdir) ||
        !resolve_add_cdb_uint(c, "env_overwrite", e->env_overwrite) ||
        !resolve_add_cdb_uint(c, "importfile", e->importfile) ||
        !resolve_add_cdb_uint(c, "nimportfile", e->nimportfile))
            return 0 ;

    return 1 ;
}
