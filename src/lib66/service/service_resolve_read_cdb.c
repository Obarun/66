/*
 * service_resolve_read_cdb.c
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

#include <stdlib.h>//free
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/cdb.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>

static int resolve_get_key_u64(const ocdb *c, const char *key, uint64_t *field)
{
    size_t klen = strlen(key) ;
    ocdb_data cdata ;

    int r = ocdb_find(c, &cdata, key, klen) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_ZERO,"search on cdb key: ",key) ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"unknown cdb key: ",key) ;

    char pack[cdata.len + 1] ;
    memcpy(pack,cdata.s, cdata.len) ;
    pack[cdata.len] = 0 ;

    u64_unpack_big(pack, field) ;

    return 1 ;

}

int service_resolve_read_cdb(ocdb *c, resolve_service_t *res)
{
    log_flow() ;


    if (resolve_get_sa(&res->sa,c) <= 0)
        return (errno = EINVAL, 0)  ;

    if (!res->sa.len)
        return (errno = EINVAL, 0)  ;

    /* configuration */
    if (!resolve_get_key(c, "rversion", &res->rversion))
        return (errno = EINVAL, 0)  ;

    if (!resolve_get_key(c, "name", &res->name) ||
        !resolve_get_key(c, "description", &res->description) ||
        !resolve_get_key(c, "version", &res->version) ||
        !resolve_get_key(c, "type", &res->type) ||
        !resolve_get_key(c, "notify", &res->notify) ||
        !resolve_get_key(c, "maxdeath", &res->maxdeath) ||
        !resolve_get_key(c, "maxdeathtime", &res->maxdeathtime) ||
        !resolve_get_key(c, "earlier", &res->earlier) ||
        !resolve_get_key(c, "copyfrom", &res->copyfrom) ||
        !resolve_get_key(c, "intree", &res->intree) ||
        !resolve_get_key(c, "ownerstr", &res->ownerstr) ||
        !resolve_get_key(c, "owner", &res->owner) ||
        !resolve_get_key(c, "treename", &res->treename) ||
        !resolve_get_key(c, "user", &res->user) ||
        !resolve_get_key(c, "inns", &res->inns) ||
        !resolve_get_key(c, "enabled", &res->enabled) ||
        !resolve_get_key(c, "islog", &res->islog) ||
        !resolve_get_key(c, "has_limit", &res->has_limit) ||

    /* path configuration */
        !resolve_get_key(c, "home", &res->path.home) ||
        !resolve_get_key(c, "frontend", &res->path.frontend) ||
        !resolve_get_key(c, "src_servicedir", &res->path.servicedir) ||

    /* dependencies */
        !resolve_get_key(c, "depends", &res->dependencies.depends) ||
        !resolve_get_key(c, "requiredby", &res->dependencies.requiredby) ||
        !resolve_get_key(c, "optsdeps", &res->dependencies.optsdeps) ||
        !resolve_get_key(c, "contents", &res->dependencies.contents) ||
        !resolve_get_key(c, "provide", &res->dependencies.provide) ||
        !resolve_get_key(c, "conflict", &res->dependencies.conflict) ||
        !resolve_get_key(c, "ndepends", &res->dependencies.ndepends) ||
        !resolve_get_key(c, "nrequiredby", &res->dependencies.nrequiredby) ||
        !resolve_get_key(c, "noptsdeps", &res->dependencies.noptsdeps) ||
        !resolve_get_key(c, "ncontents", &res->dependencies.ncontents) ||
        !resolve_get_key(c, "nprovide", &res->dependencies.nprovide) ||
        !resolve_get_key(c, "nconflict", &res->dependencies.nconflict) ||

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
        !resolve_get_key(c, "blockprivileges", &res->execute.blockprivileges) ||
        !resolve_get_key(c, "umask", &res->execute.umask) ||
        !resolve_get_key(c, "want_umask", &res->execute.want_umask) ||
        !resolve_get_key(c, "nice", &res->execute.nice) ||
        !resolve_get_key(c, "want_nice", &res->execute.want_nice) ||
        !resolve_get_key(c, "chdir", &res->execute.chdir) ||
        !resolve_get_key(c, "capsbound", &res->execute.capsbound) ||
        !resolve_get_key(c, "capsambient", &res->execute.capsambient) ||
        !resolve_get_key(c, "ncapsbound", &res->execute.ncapsbound) ||
        !resolve_get_key(c, "ncapsambient", &res->execute.ncapsambient) ||

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
        !resolve_get_key(c, "logbackup", &res->logger.backup) ||
        !resolve_get_key(c, "logmaxsize", &res->logger.maxsize) ||
        !resolve_get_key(c, "logtimestamp", &res->logger.timestamp) ||
        !resolve_get_key(c, "logwant", &res->logger.want) ||
        !resolve_get_key(c, "logrun", &res->logger.execute.run.run) ||
        !resolve_get_key(c, "logrun_user", &res->logger.execute.run.run_user) ||
        !resolve_get_key(c, "logrun_build", &res->logger.execute.run.build) ||
        !resolve_get_key(c, "logrun_runas", &res->logger.execute.run.runas) ||
        !resolve_get_key(c, "logtimeoutstart", &res->logger.execute.timeout.start) ||
        !resolve_get_key(c, "logtimeoutstop", &res->logger.execute.timeout.stop) ||

    /* environment */
        !resolve_get_key(c, "env", &res->environ.env) ||
        !resolve_get_key(c, "envdir", &res->environ.envdir) ||
        !resolve_get_key(c, "env_overwrite", &res->environ.env_overwrite) ||
        !resolve_get_key(c, "importfile", &res->environ.importfile) ||
        !resolve_get_key(c, "nimportfile", &res->environ.nimportfile) ||

    /* regex */
        !resolve_get_key(c, "configure", &res->regex.configure) ||
        !resolve_get_key(c, "directories", &res->regex.directories) ||
        !resolve_get_key(c, "files", &res->regex.files) ||
        !resolve_get_key(c, "infiles", &res->regex.infiles) ||
        !resolve_get_key(c, "ndirectories", &res->regex.ndirectories) ||
        !resolve_get_key(c, "nfiles", &res->regex.nfiles) ||
        !resolve_get_key(c, "ninfiles", &res->regex.ninfiles) ||

    /* io */
        !resolve_get_key(c, "stdintype", &res->io.fdin.type) ||
        !resolve_get_key(c, "stdindest", &res->io.fdin.destination) ||
        !resolve_get_key(c, "stdouttype", &res->io.fdout.type) ||
        !resolve_get_key(c, "stdoutdest", &res->io.fdout.destination) ||
        !resolve_get_key(c, "stderrtype", &res->io.fderr.type) ||
        !resolve_get_key(c, "stderrdest", &res->io.fderr.destination)) {
            return (errno = EINVAL, 0)  ;
    }


    return 1 ;
}

int service_resolve_read_addon_limit_cdb(ocdb *c, resolve_service_addon_limit_t *l)
{
    log_flow() ;

    if (resolve_get_sa(&l->sa, c) <= 0 || !l->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &l->rversion) ||
        !resolve_get_key_u64(c, "limitas", &l->limitas) ||
        !resolve_get_key_u64(c, "limitcore", &l->limitcore) ||
        !resolve_get_key_u64(c, "limitcpu", &l->limitcpu) ||
        !resolve_get_key_u64(c, "limitdata", &l->limitdata) ||
        !resolve_get_key_u64(c, "limitfsize", &l->limitfsize) ||
        !resolve_get_key_u64(c, "limitlocks", &l->limitlocks) ||
        !resolve_get_key_u64(c, "limitmemlock", &l->limitmemlock) ||
        !resolve_get_key_u64(c, "limitmsgqueue", &l->limitmsgqueue) ||
        !resolve_get_key_u64(c, "limitnice", &l->limitnice) ||
        !resolve_get_key_u64(c, "limitnofile", &l->limitnofile) ||
        !resolve_get_key_u64(c, "limitnproc", &l->limitnproc) ||
        !resolve_get_key_u64(c, "limitrtprio", &l->limitrtprio) ||
        !resolve_get_key_u64(c, "limitrttime", &l->limitrttime) ||
        !resolve_get_key_u64(c, "limitsigpending", &l->limitsigpending) ||
        !resolve_get_key_u64(c, "limitstack", &l->limitstack))
            return (errno = EINVAL, 0) ;

    return 1 ;
}
