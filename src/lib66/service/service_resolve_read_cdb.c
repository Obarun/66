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
        !resolve_get_key(c, "has_environ", &res->has_environ) ||
        !resolve_get_key(c, "has_io", &res->has_io) ||
        !resolve_get_key(c, "has_logger", &res->has_logger) ||
        !resolve_get_key(c, "has_execute", &res->has_execute) ||
        !resolve_get_key(c, "has_dependencies", &res->has_dependencies) ||

    /* path configuration */
        !resolve_get_key(c, "home", &res->path.home) ||
        !resolve_get_key(c, "frontend", &res->path.frontend) ||
        !resolve_get_key(c, "src_servicedir", &res->path.servicedir) ||

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

    /* regex */
        !resolve_get_key(c, "configure", &res->regex.configure) ||
        !resolve_get_key(c, "directories", &res->regex.directories) ||
        !resolve_get_key(c, "files", &res->regex.files) ||
        !resolve_get_key(c, "infiles", &res->regex.infiles) ||
        !resolve_get_key(c, "ndirectories", &res->regex.ndirectories) ||
        !resolve_get_key(c, "nfiles", &res->regex.nfiles) ||
        !resolve_get_key(c, "ninfiles", &res->regex.ninfiles)) {
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

int service_resolve_read_addon_environ_cdb(ocdb *c, resolve_service_addon_environ_t *e)
{
    log_flow() ;

    if (resolve_get_sa(&e->sa, c) <= 0 || !e->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &e->rversion) ||
        !resolve_get_key(c, "env", &e->env) ||
        !resolve_get_key(c, "envdir", &e->envdir) ||
        !resolve_get_key(c, "env_overwrite", &e->env_overwrite) ||
        !resolve_get_key(c, "importfile", &e->importfile) ||
        !resolve_get_key(c, "nimportfile", &e->nimportfile))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_io_cdb(ocdb *c, resolve_service_addon_io_t *io)
{
    log_flow() ;

    if (resolve_get_sa(&io->sa, c) <= 0 || !io->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &io->rversion) ||
        !resolve_get_key(c, "stdintype", &io->fdin.type) ||
        !resolve_get_key(c, "stdindest", &io->fdin.destination) ||
        !resolve_get_key(c, "stdouttype", &io->fdout.type) ||
        !resolve_get_key(c, "stdoutdest", &io->fdout.destination) ||
        !resolve_get_key(c, "stderrtype", &io->fderr.type) ||
        !resolve_get_key(c, "stderrdest", &io->fderr.destination))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_logger_cdb(ocdb *c, resolve_service_addon_logger_t *lg)
{
    log_flow() ;

    if (resolve_get_sa(&lg->sa, c) <= 0 || !lg->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &lg->rversion) ||
        !resolve_get_key(c, "logbackup", &lg->backup) ||
        !resolve_get_key(c, "logmaxsize", &lg->maxsize) ||
        !resolve_get_key(c, "logtimestamp", &lg->timestamp) ||
        !resolve_get_key(c, "logrun", &lg->execute.run.run) ||
        !resolve_get_key(c, "logrun_user", &lg->execute.run.run_user) ||
        !resolve_get_key(c, "logrun_build", &lg->execute.run.build) ||
        !resolve_get_key(c, "logrun_runas", &lg->execute.run.runas) ||
        !resolve_get_key(c, "logtimeoutstart", &lg->execute.timeout.start) ||
        !resolve_get_key(c, "logtimeoutstop", &lg->execute.timeout.stop))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_execute_cdb(ocdb *c, resolve_service_addon_execute_t *ex)
{
    log_flow() ;

    if (resolve_get_sa(&ex->sa, c) <= 0 || !ex->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &ex->rversion) ||
        !resolve_get_key(c, "notify", &ex->notify) ||
        !resolve_get_key(c, "maxdeath", &ex->maxdeath) ||
        !resolve_get_key(c, "maxdeathtime", &ex->maxdeathtime) ||
        !resolve_get_key(c, "run", &ex->run.run) ||
        !resolve_get_key(c, "run_user", &ex->run.run_user) ||
        !resolve_get_key(c, "run_build", &ex->run.build) ||
        !resolve_get_key(c, "run_runas", &ex->run.runas) ||
        !resolve_get_key(c, "finish", &ex->finish.run) ||
        !resolve_get_key(c, "finish_user", &ex->finish.run_user) ||
        !resolve_get_key(c, "finish_build", &ex->finish.build) ||
        !resolve_get_key(c, "finish_runas", &ex->finish.runas) ||
        !resolve_get_key(c, "timeoutstart", &ex->timeout.start) ||
        !resolve_get_key(c, "timeoutstop", &ex->timeout.stop) ||
        !resolve_get_key(c, "down", &ex->down) ||
        !resolve_get_key(c, "downsignal", &ex->downsignal) ||
        !resolve_get_key(c, "blockprivileges", &ex->blockprivileges) ||
        !resolve_get_key(c, "umask", &ex->umask) ||
        !resolve_get_key(c, "want_umask", &ex->want_umask) ||
        !resolve_get_key(c, "nice", &ex->nice) ||
        !resolve_get_key(c, "want_nice", &ex->want_nice) ||
        !resolve_get_key(c, "chdir", &ex->chdir) ||
        !resolve_get_key(c, "capsbound", &ex->capsbound) ||
        !resolve_get_key(c, "capsambient", &ex->capsambient) ||
        !resolve_get_key(c, "ncapsbound", &ex->ncapsbound) ||
        !resolve_get_key(c, "ncapsambient", &ex->ncapsambient))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_dependencies_cdb(ocdb *c, resolve_service_addon_dependencies_t *dep)
{
    log_flow() ;

    if (resolve_get_sa(&dep->sa, c) <= 0 || !dep->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key(c, "rversion", &dep->rversion) ||
        !resolve_get_key(c, "depends", &dep->depends) ||
        !resolve_get_key(c, "requiredby", &dep->requiredby) ||
        !resolve_get_key(c, "optsdeps", &dep->optsdeps) ||
        !resolve_get_key(c, "contents", &dep->contents) ||
        !resolve_get_key(c, "provide", &dep->provide) ||
        !resolve_get_key(c, "conflict", &dep->conflict) ||
        !resolve_get_key(c, "ndepends", &dep->ndepends) ||
        !resolve_get_key(c, "nrequiredby", &dep->nrequiredby) ||
        !resolve_get_key(c, "noptsdeps", &dep->noptsdeps) ||
        !resolve_get_key(c, "ncontents", &dep->ncontents) ||
        !resolve_get_key(c, "nprovide", &dep->nprovide) ||
        !resolve_get_key(c, "nconflict", &dep->nconflict))
            return (errno = EINVAL, 0) ;

    return 1 ;
}
