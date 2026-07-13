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

int service_resolve_read_cdb(ocdb *c, resolve_service_t *res)
{
    log_flow() ;


    if (resolve_get_sa(&res->sa,c) <= 0)
        return (errno = EINVAL, 0)  ;

    if (!res->sa.len)
        return (errno = EINVAL, 0)  ;

    /* configuration */
    if (!resolve_get_key_u32(c, "rversion", &res->rversion))
        return (errno = EINVAL, 0)  ;

    if (!resolve_get_key_u32(c, "name", &res->name) ||
        !resolve_get_key_u32(c, "description", &res->description) ||
        !resolve_get_key_u32(c, "version", &res->version) ||
        !resolve_get_key_u32(c, "type", &res->type) ||
        !resolve_get_key_u32(c, "earlier", &res->earlier) ||
        !resolve_get_key_u32(c, "copyfrom", &res->copyfrom) ||
        !resolve_get_key_u32(c, "intree", &res->intree) ||
        !resolve_get_key_u32(c, "ownerstr", &res->ownerstr) ||
        !resolve_get_key_u32(c, "owner", &res->owner) ||
        !resolve_get_key_u32(c, "treename", &res->treename) ||
        !resolve_get_key_u32(c, "user", &res->user) ||
        !resolve_get_key_u32(c, "inns", &res->inns) ||
        !resolve_get_key_u32(c, "enabled", &res->enabled) ||
        !resolve_get_key_u32(c, "islog", &res->islog) ||
        !resolve_get_key_u32(c, "logger", &res->logger) ||
        !resolve_get_key_u32(c, "has_limit", &res->has_limit) ||
        !resolve_get_key_u32(c, "has_environ", &res->has_environ) ||
        !resolve_get_key_u32(c, "has_io", &res->has_io) ||
        !resolve_get_key_u32(c, "has_execute", &res->has_execute) ||
        !resolve_get_key_u32(c, "has_dependencies", &res->has_dependencies) ||
        !resolve_get_key_u32(c, "has_regex", &res->has_regex) ||
        !resolve_get_key_u32(c, "has_event", &res->has_event) ||

    /* path configuration */
        !resolve_get_key_u32(c, "home", &res->path.home) ||
        !resolve_get_key_u32(c, "frontend", &res->path.frontend) ||
        !resolve_get_key_u32(c, "src_servicedir", &res->path.servicedir) ||

    /* live */
        !resolve_get_key_u32(c, "livedir", &res->live.livedir) ||
        !resolve_get_key_u32(c, "status", &res->live.status) ||
        !resolve_get_key_u32(c, "live_servicedir", &res->live.servicedir) ||
        !resolve_get_key_u32(c, "scandir", &res->live.scandir) ||
        !resolve_get_key_u32(c, "statedir", &res->live.statedir) ||
        !resolve_get_key_u32(c, "eventdir", &res->live.eventdir) ||
        !resolve_get_key_u32(c, "supervisedir", &res->live.supervisedir) ||
        !resolve_get_key_u32(c, "fdholderdir", &res->live.fdholderdir) ||
        !resolve_get_key_u32(c, "oneshotddir", &res->live.oneshotddir) ||
        !resolve_get_key_u32(c, "eventddir", &res->live.eventddir)) {
            return (errno = EINVAL, 0)  ;
    }


    return 1 ;
}

int service_resolve_read_addon_limit_cdb(ocdb *c, resolve_service_addon_limit_t *l)
{
    log_flow() ;

    if (resolve_get_sa(&l->sa, c) <= 0 || !l->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &l->rversion) ||
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

    if (!resolve_get_key_u32(c, "rversion", &e->rversion) ||
        !resolve_get_key_u32(c, "env", &e->env) ||
        !resolve_get_key_u32(c, "envdir", &e->envdir) ||
        !resolve_get_key_u32(c, "env_overwrite", &e->env_overwrite) ||
        !resolve_get_key_u32(c, "importfile", &e->importfile) ||
        !resolve_get_key_u32(c, "nimportfile", &e->nimportfile))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_io_cdb(ocdb *c, resolve_service_addon_io_t *io)
{
    log_flow() ;

    if (resolve_get_sa(&io->sa, c) <= 0 || !io->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &io->rversion) ||
        !resolve_get_key_u32(c, "stdintype", &io->fdin.type) ||
        !resolve_get_key_u32(c, "stdindest", &io->fdin.destination) ||
        !resolve_get_key_u32(c, "stdouttype", &io->fdout.type) ||
        !resolve_get_key_u32(c, "stdoutdest", &io->fdout.destination) ||
        !resolve_get_key_u32(c, "stderrtype", &io->fderr.type) ||
        !resolve_get_key_u32(c, "stderrdest", &io->fderr.destination) ||
        !resolve_get_key_u32(c, "iorunas", &io->runas))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_execute_cdb(ocdb *c, resolve_service_addon_execute_t *ex)
{
    log_flow() ;

    if (resolve_get_sa(&ex->sa, c) <= 0 || !ex->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &ex->rversion) ||
        !resolve_get_key_u32(c, "notify", &ex->notify) ||
        !resolve_get_key_u32(c, "maxdeath", &ex->maxdeath) ||
        !resolve_get_key_u32(c, "maxdeathtime", &ex->maxdeathtime) ||
        !resolve_get_key_u32(c, "run", &ex->run.run) ||
        !resolve_get_key_u32(c, "run_user", &ex->run.run_user) ||
        !resolve_get_key_u32(c, "run_build", &ex->run.build) ||
        !resolve_get_key_u32(c, "run_runas", &ex->run.runas) ||
        !resolve_get_key_u32(c, "finish", &ex->finish.run) ||
        !resolve_get_key_u32(c, "finish_user", &ex->finish.run_user) ||
        !resolve_get_key_u32(c, "finish_build", &ex->finish.build) ||
        !resolve_get_key_u32(c, "finish_runas", &ex->finish.runas) ||
        !resolve_get_key_u32(c, "timeoutstart", &ex->timeout.start) ||
        !resolve_get_key_u32(c, "timeoutstop", &ex->timeout.stop) ||
        !resolve_get_key_u32(c, "down", &ex->down) ||
        !resolve_get_key_u32(c, "downsignal", &ex->downsignal) ||
        !resolve_get_key_u32(c, "blockprivileges", &ex->blockprivileges) ||
        !resolve_get_key_u32(c, "umask", &ex->umask) ||
        !resolve_get_key_u32(c, "want_umask", &ex->want_umask) ||
        !resolve_get_key_u32(c, "nice", &ex->nice) ||
        !resolve_get_key_u32(c, "want_nice", &ex->want_nice) ||
        !resolve_get_key_u32(c, "chdir", &ex->chdir) ||
        !resolve_get_key_u32(c, "capsbound", &ex->capsbound) ||
        !resolve_get_key_u32(c, "capsambient", &ex->capsambient) ||
        !resolve_get_key_u32(c, "ncapsbound", &ex->ncapsbound) ||
        !resolve_get_key_u32(c, "ncapsambient", &ex->ncapsambient))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_dependencies_cdb(ocdb *c, resolve_service_addon_dependencies_t *dep)
{
    log_flow() ;

    if (resolve_get_sa(&dep->sa, c) <= 0 || !dep->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &dep->rversion) ||
        !resolve_get_key_u32(c, "depends", &dep->depends) ||
        !resolve_get_key_u32(c, "requiredby", &dep->requiredby) ||
        !resolve_get_key_u32(c, "optsdeps", &dep->optsdeps) ||
        !resolve_get_key_u32(c, "contents", &dep->contents) ||
        !resolve_get_key_u32(c, "provide", &dep->provide) ||
        !resolve_get_key_u32(c, "conflict", &dep->conflict) ||
        !resolve_get_key_u32(c, "ndepends", &dep->ndepends) ||
        !resolve_get_key_u32(c, "nrequiredby", &dep->nrequiredby) ||
        !resolve_get_key_u32(c, "noptsdeps", &dep->noptsdeps) ||
        !resolve_get_key_u32(c, "ncontents", &dep->ncontents) ||
        !resolve_get_key_u32(c, "nprovide", &dep->nprovide) ||
        !resolve_get_key_u32(c, "nconflict", &dep->nconflict))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_regex_cdb(ocdb *c, resolve_service_addon_regex_t *rx)
{
    log_flow() ;

    if (resolve_get_sa(&rx->sa, c) <= 0 || !rx->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &rx->rversion) ||
        !resolve_get_key_u32(c, "configure", &rx->configure) ||
        !resolve_get_key_u32(c, "directories", &rx->directories) ||
        !resolve_get_key_u32(c, "files", &rx->files) ||
        !resolve_get_key_u32(c, "infiles", &rx->infiles) ||
        !resolve_get_key_u32(c, "ndirectories", &rx->ndirectories) ||
        !resolve_get_key_u32(c, "nfiles", &rx->nfiles) ||
        !resolve_get_key_u32(c, "ninfiles", &rx->ninfiles))
            return (errno = EINVAL, 0) ;

    return 1 ;
}

int service_resolve_read_addon_event_cdb(ocdb *c, resolve_service_addon_event_t *ev)
{
    log_flow() ;

    if (resolve_get_sa(&ev->sa, c) <= 0 || !ev->sa.len)
        return (errno = EINVAL, 0) ;

    if (!resolve_get_key_u32(c, "rversion", &ev->rversion) ||
        !resolve_get_key_u32(c, "eventtype", &ev->type) ||
        !resolve_get_key_u32(c, "eventfrom", &ev->from) ||
        !resolve_get_key_u32(c, "neventfrom", &ev->nfrom) ||
        !resolve_get_key_u32(c, "eventfromfield", &ev->fromfield) ||
        !resolve_get_key_u32(c, "eventon", &ev->on) ||
        !resolve_get_key_u32(c, "neventon", &ev->non) ||
        !resolve_get_key_u32(c, "eventcombine", &ev->combine) ||
        !resolve_get_key_u32(c, "eventdo", &ev->docmd) ||
        !resolve_get_key_u32(c, "eventemit", &ev->emit) ||
        !resolve_get_key_u32(c, "eventwatch", &ev->watch) ||
        !resolve_get_key_u32(c, "eventexpression", &ev->expression) ||
        !resolve_get_key_u32(c, "eventtimezone", &ev->timezone) ||
        !resolve_get_key_u32(c, "eventinterval", &ev->interval))
            return (errno = EINVAL, 0) ;

    return 1 ;
}
