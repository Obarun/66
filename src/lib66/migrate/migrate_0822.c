/*
 * migrate_0821.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>//memcpy
#include <stdlib.h>//free
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/cdb.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/module.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

#include <66/migrate_0821.h>
#include <66/migrate.h>

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

int service_resolve_read_cdb_0821(ocdb *c, resolve_service_t_0821 *res)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (resolve_get_sa(&res->sa,c) <= 0) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    if (!res->sa.len) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    /* configuration */
    if (!resolve_get_key(c, "rversion", &res->rversion)) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    if (!resolve_get_key(c, "name", &res->name) ||
        !resolve_get_key(c, "description", &res->description) ||
        !resolve_get_key(c, "version", &res->version) ||
        !resolve_get_key(c, "type", &res->type) ||
        !resolve_get_key(c, "notify", &res->notify) ||
        !resolve_get_key(c, "maxdeath", &res->maxdeath) ||
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
        !resolve_get_key(c, "stderrdest", &res->io.fderr.destination) ||

    /* limit */
        !resolve_get_key_u64(c, "limitas", &res->limit.limitas) ||
        !resolve_get_key_u64(c, "limitcore", &res->limit.limitcore) ||
        !resolve_get_key_u64(c, "limitcpu", &res->limit.limitcpu) ||
        !resolve_get_key_u64(c, "limitdata", &res->limit.limitdata) ||
        !resolve_get_key_u64(c, "limitfsize", &res->limit.limitfsize) ||
        !resolve_get_key_u64(c, "limitlocks", &res->limit.limitlocks) ||
        !resolve_get_key_u64(c, "limitmemlock", &res->limit.limitmemlock) ||
        !resolve_get_key_u64(c, "limitmsgqueue", &res->limit.limitmsgqueue) ||
        !resolve_get_key_u64(c, "limitnice", &res->limit.limitnice) ||
        !resolve_get_key_u64(c, "limitnofile", &res->limit.limitnofile) ||
        !resolve_get_key_u64(c, "limitnproc", &res->limit.limitnproc) ||
        !resolve_get_key_u64(c, "limitrtprio", &res->limit.limitrtprio) ||
        !resolve_get_key_u64(c, "limitrttime", &res->limit.limitrttime) ||
        !resolve_get_key_u64(c, "limitsigpending", &res->limit.limitsigpending) ||
        !resolve_get_key_u64(c, "limitstack", &res->limit.limitstack)) {
            free(wres) ;
            return (errno = EINVAL, 0)  ;
    }

    free(wres) ;

    return 1 ;
}

static void service_resolve_sanitize_0821(resolve_service_t *new, resolve_service_addon_environ_t *e, resolve_service_addon_io_t *io, resolve_service_addon_logger_t *lg, resolve_service_addon_execute_t *ex, resolve_service_addon_dependencies_t *dep, resolve_service_addon_regex_t *rx, resolve_service_addon_limit_t *lim, resolve_service_t_0821 *old)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, new) ;

    resolve_init(wres) ;

    // config
    new->name = old->name ? resolve_add_string(wres, old->sa.s + old->name) : 0 ;
    new->description = old->description ? resolve_add_string(wres, old->sa.s + old->description) : 0 ;
    new->version = old->version ? resolve_add_string(wres, old->sa.s + old->version) : 0 ;
    new->type = old->type ;
    new->earlier = old->earlier ;
    new->copyfrom = old->copyfrom ? resolve_add_string(wres, old->sa.s + old->copyfrom) : 0 ;
    new->intree = old->intree ? resolve_add_string(wres, old->sa.s + old->intree) : 0 ;
    new->ownerstr = old->ownerstr ? resolve_add_string(wres, old->sa.s + old->ownerstr) : 0 ;
    new->owner = old->owner ;
    new->treename = old->treename ? resolve_add_string(wres, old->sa.s + old->treename) : 0 ;
    new->user = old->user ? resolve_add_string(wres, old->sa.s + old->user) : 0 ;
    new->inns = old->inns ? resolve_add_string(wres, old->sa.s + old->inns) : 0 ;
    new->enabled = old->enabled ;
    new->islog = old->islog ;

    // path
    new->path.home = old->path.home ? resolve_add_string(wres, old->sa.s + old->path.home) : 0 ;
    new->path.frontend = old->path.frontend ? resolve_add_string(wres, old->sa.s + old->path.frontend) : 0 ;
    new->path.servicedir = old->path.servicedir ? resolve_add_string(wres, old->sa.s + old->path.servicedir) : 0 ;

    // dependencies -> autonomous addon
    new->has_dependencies = (old->dependencies.ndepends || old->dependencies.nrequiredby ||
                             old->dependencies.noptsdeps || old->dependencies.ncontents ||
                             old->dependencies.nprovide || old->dependencies.nconflict) ? 1 : 0 ;
    if (new->has_dependencies) {
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
        resolve_init(depwres) ;
        dep->depends = old->dependencies.depends ? resolve_add_string(depwres, old->sa.s + old->dependencies.depends) : 0 ;
        dep->requiredby = old->dependencies.requiredby ? resolve_add_string(depwres, old->sa.s + old->dependencies.requiredby) : 0 ;
        dep->optsdeps = old->dependencies.optsdeps ? resolve_add_string(depwres, old->sa.s + old->dependencies.optsdeps) : 0 ;
        dep->contents = old->dependencies.contents ? resolve_add_string(depwres, old->sa.s + old->dependencies.contents) : 0 ;
        dep->provide = old->dependencies.provide ? resolve_add_string(depwres, old->sa.s + old->dependencies.provide) : 0 ;
        dep->conflict = old->dependencies.conflict ? resolve_add_string(depwres, old->sa.s + old->dependencies.conflict) : 0 ;
        dep->ndepends = old->dependencies.ndepends ;
        dep->nrequiredby = old->dependencies.nrequiredby ;
        dep->noptsdeps = old->dependencies.noptsdeps ;
        dep->ncontents = old->dependencies.ncontents ;
        dep->nprovide = old->dependencies.nprovide ;
        dep->nconflict = old->dependencies.nconflict ;
        free(depwres) ;
    }

    // execute -> autonomous addon (notify/maxdeath/maxdeathtime relocated here)
    new->has_execute = 1 ;
    {
        resolve_wrapper_t_ref exwres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;
        resolve_init(exwres) ;
        ex->run.run = old->execute.run.run ? resolve_add_string(exwres, old->sa.s + old->execute.run.run) : 0 ;
        ex->run.run_user = old->execute.run.run_user ? resolve_add_string(exwres, old->sa.s + old->execute.run.run_user) : 0 ;
        ex->run.build = old->execute.run.build ? resolve_add_string(exwres, old->sa.s + old->execute.run.build) : 0 ;
        ex->run.runas = old->execute.run.runas ? resolve_add_string(exwres, old->sa.s + old->execute.run.runas) : 0 ;
        ex->finish.run = old->execute.finish.run ? resolve_add_string(exwres, old->sa.s + old->execute.finish.run) : 0 ;
        ex->finish.run_user = old->execute.finish.run_user ? resolve_add_string(exwres, old->sa.s + old->execute.finish.run_user) : 0 ;
        ex->finish.build = old->execute.finish.build ? resolve_add_string(exwres, old->sa.s + old->execute.finish.build) : 0 ;
        ex->finish.runas = old->execute.finish.runas ? resolve_add_string(exwres, old->sa.s + old->execute.finish.runas) : 0 ;
        ex->timeout.start = old->execute.timeout.start ;
        ex->timeout.stop = old->execute.timeout.stop ;
        ex->notify = old->notify ;
        ex->maxdeath = old->maxdeath ;
        /* maxdeathtime is not part of the released 0.8.2.x schema; keep the split
         * execute addon default (RESOLVE_SERVICE_ADDON_EXECUTE_ZERO). */
        ex->down = old->execute.down ;
        ex->downsignal = old->execute.downsignal ;
        ex->blockprivileges = old->execute.blockprivileges ;
        ex->umask = old->execute.umask ;
        ex->want_umask = old->execute.want_umask ;
        ex->nice = old->execute.nice ;
        ex->want_nice = old->execute.want_nice ;
        ex->chdir = old->execute.chdir ? resolve_add_string(exwres, old->sa.s + old->execute.chdir) : 0 ;
        ex->capsbound = old->execute.capsbound ? resolve_add_string(exwres, old->sa.s + old->execute.capsbound) : 0 ;
        ex->capsambient = old->execute.capsambient ? resolve_add_string(exwres, old->sa.s + old->execute.capsambient) : 0 ;
        ex->ncapsbound = old->execute.ncapsbound ;
        ex->ncapsambient = old->execute.ncapsambient ;
        free(exwres) ;
    }

    // live (notifdir dropped from the core)
    new->live.livedir = old->live.livedir ? resolve_add_string(wres, old->sa.s + old->live.livedir) : 0 ;
    new->live.status = old->live.status ? resolve_add_string(wres, old->sa.s + old->live.status) : 0 ;
    new->live.servicedir = old->live.servicedir ? resolve_add_string(wres, old->sa.s + old->live.servicedir) : 0 ;
    new->live.scandir = old->live.scandir ? resolve_add_string(wres, old->sa.s + old->live.scandir) : 0 ;
    new->live.statedir = old->live.statedir ? resolve_add_string(wres, old->sa.s + old->live.statedir) : 0 ;
    new->live.eventdir = old->live.eventdir ? resolve_add_string(wres, old->sa.s + old->live.eventdir) : 0 ;
    new->live.supervisedir = old->live.supervisedir ? resolve_add_string(wres, old->sa.s + old->live.supervisedir) : 0 ;
    new->live.fdholderdir = old->live.fdholderdir ? resolve_add_string(wres, old->sa.s + old->live.fdholderdir) : 0 ;
    new->live.oneshotddir = old->live.oneshotddir ? resolve_add_string(wres, old->sa.s + old->live.oneshotddir) : 0 ;

    // logger -> core boolean flag + transient addon (log-dir owner moves onto io)
    new->logger = old->logger.want ? 1 : 0 ;
    if (new->logger) {
        resolve_wrapper_t_ref lgwres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;
        resolve_init(lgwres) ;
        lg->backup = old->logger.backup ;
        lg->maxsize = old->logger.maxsize ;
        lg->timestamp = old->logger.timestamp ;
        lg->execute.run.run = old->logger.execute.run.run ? resolve_add_string(lgwres, old->sa.s + old->logger.execute.run.run) : 0 ;
        lg->execute.run.run_user = old->logger.execute.run.run_user ? resolve_add_string(lgwres, old->sa.s + old->logger.execute.run.run_user) : 0 ;
        lg->execute.run.build = old->logger.execute.run.build ? resolve_add_string(lgwres, old->sa.s + old->logger.execute.run.build) : 0 ;
        lg->execute.run.runas = old->logger.execute.run.runas ? resolve_add_string(lgwres, old->sa.s + old->logger.execute.run.runas) : 0 ;
        lg->execute.timeout.start = old->logger.execute.timeout.start ;
        lg->execute.timeout.stop = old->logger.execute.timeout.stop ;
        free(lgwres) ;
    }

    // environ -> autonomous addon (routed out of the core)
    new->has_environ = old->environ.env ? 1 : 0 ;
    if (new->has_environ) {
        resolve_wrapper_t_ref ewres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;
        resolve_init(ewres) ;
        e->env = old->environ.env ? resolve_add_string(ewres, old->sa.s + old->environ.env) : 0 ;
        e->envdir = old->environ.envdir ? resolve_add_string(ewres, old->sa.s + old->environ.envdir) : 0 ;
        e->env_overwrite = old->environ.env_overwrite ;
        e->importfile = old->environ.importfile ? resolve_add_string(ewres, old->sa.s + old->environ.importfile) : 0 ;
        e->nimportfile = old->environ.nimportfile ;
        free(ewres) ;
    }

    // regex
    new->has_regex = (old->regex.configure || old->regex.directories || old->regex.files ||
                      old->regex.infiles || old->regex.ndirectories || old->regex.nfiles ||
                      old->regex.ninfiles) ? 1 : 0 ;
    if (new->has_regex) {
        resolve_wrapper_t_ref rxwres = resolve_set_struct(DATA_SERVICE_REGEX, rx) ;
        resolve_init(rxwres) ;
        rx->configure = old->regex.configure ? resolve_add_string(rxwres, old->sa.s + old->regex.configure) : 0 ;
        rx->directories = old->regex.directories ? resolve_add_string(rxwres, old->sa.s + old->regex.directories) : 0 ;
        rx->files = old->regex.files ? resolve_add_string(rxwres, old->sa.s + old->regex.files) : 0 ;
        rx->infiles = old->regex.infiles ? resolve_add_string(rxwres, old->sa.s + old->regex.infiles) : 0 ;
        rx->ndirectories = old->regex.ndirectories ;
        rx->nfiles = old->regex.nfiles ;
        rx->ninfiles = old->regex.ninfiles ;
        free(rxwres) ;
    }

    // io
    new->has_io = 1 ;
    {
        resolve_wrapper_t_ref iowres = resolve_set_struct(DATA_SERVICE_IO, io) ;
        resolve_init(iowres) ;
        io->fdin.type = old->io.fdin.type ;
        io->fdin.destination = old->io.fdin.destination ? resolve_add_string(iowres, old->sa.s + old->io.fdin.destination) : 0 ;
        io->fdout.type = old->io.fdout.type ;
        io->fdout.destination = old->io.fdout.destination ? resolve_add_string(iowres, old->sa.s + old->io.fdout.destination) : 0 ;
        io->fderr.type = old->io.fderr.type ;
        io->fderr.destination = old->io.fderr.destination ? resolve_add_string(iowres, old->sa.s + old->io.fderr.destination) : 0 ;
        free(iowres) ;
    }

    // limit -> autonomous addon. A limit of 0 means "not applied" (66-execute skips
    // it), so a service is gated on at least one non-zero rlimit.
    new->has_limit = (old->limit.limitas || old->limit.limitcore || old->limit.limitcpu ||
                      old->limit.limitdata || old->limit.limitfsize || old->limit.limitlocks ||
                      old->limit.limitmemlock || old->limit.limitmsgqueue || old->limit.limitnice ||
                      old->limit.limitnofile || old->limit.limitnproc || old->limit.limitrtprio ||
                      old->limit.limitrttime || old->limit.limitsigpending || old->limit.limitstack) ? 1 : 0 ;
    if (new->has_limit) {
        resolve_wrapper_t_ref limwres = resolve_set_struct(DATA_SERVICE_LIMIT, lim) ;
        resolve_init(limwres) ;
        lim->limitas = old->limit.limitas ;
        lim->limitcore = old->limit.limitcore ;
        lim->limitcpu = old->limit.limitcpu ;
        lim->limitdata = old->limit.limitdata ;
        lim->limitfsize = old->limit.limitfsize ;
        lim->limitlocks = old->limit.limitlocks ;
        lim->limitmemlock = old->limit.limitmemlock ;
        lim->limitmsgqueue = old->limit.limitmsgqueue ;
        lim->limitnice = old->limit.limitnice ;
        lim->limitnofile = old->limit.limitnofile ;
        lim->limitnproc = old->limit.limitnproc ;
        lim->limitrtprio = old->limit.limitrtprio ;
        lim->limitrttime = old->limit.limitrttime ;
        lim->limitsigpending = old->limit.limitsigpending ;
        lim->limitstack = old->limit.limitstack ;
        free(limwres) ;
    }

    free(wres) ;
}

static void migrate_resolve(ssexec_t *info, const char *path, const char *name)
{
    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_t_0821 res = RESOLVE_SERVICE_ZERO_0821 ;
    resolve_service_t new = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &new) ;

    if (resolve_open_cdb(&fd, &c, path, name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "open resolve file of service: ", name) ;

    if (!service_resolve_read_cdb_0821(&c, &res))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of service: ", name) ;

    resolve_service_addon_environ_t environ = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_service_addon_logger_t logger = RESOLVE_SERVICE_ADDON_LOGGER_ZERO ;
    resolve_service_addon_execute_t execute = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_service_addon_dependencies_t dependencies = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_service_addon_regex_t regex = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_service_addon_limit_t limit = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    service_resolve_sanitize_0821(&new, &environ, &io, &logger, &execute, &dependencies, &regex, &limit, &res) ;

    /* the logger companion is now a classic-only flag, and there is no logger
     * addon: the log-dir owner (ex the logger runas) moves onto the io addon. */
    if (new.type != E_PARSER_TYPE_CLASSIC)
        new.logger = 0 ;

    if (res.logger.want && res.logger.execute.run.runas && io.fdout.type == E_PARSER_IO_TYPE_66LOG) {
        resolve_wrapper_t_ref iow = resolve_set_struct(DATA_SERVICE_IO, &io) ;
        io.runas = resolve_add_string(iow, res.sa.s + res.logger.execute.run.runas) ;
        free(iow) ;
    }

    migrate_ensure_log_owner(&new, &io, &logger) ;

    if (!resolve_write(wres, info->base.s, name))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

    if (new.has_environ) {
        resolve_wrapper_t_ref we = resolve_set_struct(DATA_SERVICE_ENVIRON, &environ) ;
        if (!resolve_write(we, info->base.s, name)) {
            resolve_free(we) ;
            log_dieusys(LOG_EXIT_SYS, "write environ addon of service: ", name) ;
        }
        resolve_free(we) ;
    }

    if (new.has_io) {
        resolve_wrapper_t_ref wio = resolve_set_struct(DATA_SERVICE_IO, &io) ;
        if (!resolve_write(wio, info->base.s, name)) {
            resolve_free(wio) ;
            log_dieusys(LOG_EXIT_SYS, "write io addon of service: ", name) ;
        }
        resolve_free(wio) ;
    }

    if (new.has_execute) {
        resolve_wrapper_t_ref wex = resolve_set_struct(DATA_SERVICE_EXECUTE, &execute) ;
        if (!resolve_write(wex, info->base.s, name)) {
            resolve_free(wex) ;
            log_dieusys(LOG_EXIT_SYS, "write execute addon of service: ", name) ;
        }
        resolve_free(wex) ;
    }

    if (new.has_dependencies) {
        resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dependencies) ;
        if (!resolve_write(wdep, info->base.s, name)) {
            resolve_free(wdep) ;
            log_dieusys(LOG_EXIT_SYS, "write dependencies addon of service: ", name) ;
        }
        resolve_free(wdep) ;
    }

    if (new.has_regex) {
        resolve_wrapper_t_ref wrx = resolve_set_struct(DATA_SERVICE_REGEX, &regex) ;
        if (!resolve_write(wrx, info->base.s, name)) {
            resolve_free(wrx) ;
            log_dieusys(LOG_EXIT_SYS, "write regex addon of service: ", name) ;
        }
        resolve_free(wrx) ;
    }

    if (new.has_limit) {
        resolve_wrapper_t_ref wlim = resolve_set_struct(DATA_SERVICE_LIMIT, &limit) ;
        if (!resolve_write(wlim, info->base.s, name)) {
            resolve_free(wlim) ;
            log_dieusys(LOG_EXIT_SYS, "write limit addon of service: ", name) ;
        }
        resolve_free(wlim) ;
    }

    /* the logger addon is transient (only feeds migrate_ensure_log_owner, never
     * written to disk), so nothing else frees its arena -- do it here. */
    strbuf_free(&logger.sa) ;

    resolve_free(wres) ;
}

static void migrate_service_0822(void)
{
    log_flow() ;

    size_t pos = 0 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, 0 } ;
    ssexec_t info = SSEXEC_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    char path[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + SS_RESOLVE_LEN + 1 + 1] ;
    auto_strings(path, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/") ;
    size_t len = info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 ;

    if (!sbl_dir_get_recursive(&sa, path, exclude, S_IFLNK, 0))
        log_dieu(LOG_EXIT_SYS, "get resolve files") ;

    FOREACH_SBL(&sa, pos) {

        char *name = sa.s + pos ;

        auto_strings(path + len, name, SS_RESOLVE, "/") ;

        migrate_resolve(&info, path, name) ;
    }

    ssexec_free(&info) ;
}

static void migrate_scandir_resolve(const char *rdir, const char *dir, const char *name)
{
    log_flow() ;

    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_t_0821 old = RESOLVE_SERVICE_ZERO_0821 ;
    resolve_service_t new = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &new) ;
    resolve_wrapper_t_ref wex = resolve_set_struct(DATA_SERVICE_EXECUTE, &ex) ;

    int r = resolve_open_cdb(&fd, &c, rdir, name) ;
    if (r <= 0) {
        /* r == 0: no resolve here (service absent from this scandir) -- skip.
         * r < 0: present but unreadable -- fatal. */
        free(wres) ;
        free(wex) ;
        if (r < 0)
            log_warnusys(LOG_EXIT_SYS, "open resolve file of service: ", name, " -- you may need to force the reboot using 66 reboot -f") ;
        return ;
    }

    if (!service_resolve_read_cdb_0821(&c, &old))
        log_warnusys(LOG_EXIT_SYS, "read resolve file of service: ", name, " -- you may need to force the reboot using 66 reboot -f") ;

    resolve_init(wres) ;
    resolve_init(wex) ;

    new.name = old.name ? resolve_add_string(wres, old.sa.s + old.name) : resolve_add_string(wres, name) ;
    new.type = old.type ;
    new.has_execute = 1 ;
    ex.notify = old.notify ;

    if (!resolve_write_at(wres, dir, name))
        log_warnusys(LOG_EXIT_SYS, "write resolve file of service: ", name, " -- you may need to force the reboot using 66 reboot -f") ;

    {
        char aname[strlen(name) + SS_ADDON_EXECUTE_SUFFIX_LEN + 1] ;
        auto_strings(aname, name, SS_ADDON_EXECUTE_SUFFIX) ;
        if (!resolve_write_at(wex, dir, aname))
            log_warnusys(LOG_EXIT_SYS, "write execute addon of service: ", name, " -- you may need to force the reboot using 66 reboot -f") ;
    }

    strbuf_free(&old.sa) ;
    resolve_free(wres) ;
    resolve_free(wex) ;
}

static void migrate_scandir_0822(void)
{
    log_flow() ;

    static char const *const internal[] = {
        SS_BOOT_SHUTDOWND,
        SS_SCANDIR SS_LOG_SUFFIX,
        SS_ONESHOTD,
        SS_FDHOLDER,
        0
    } ;

    ssexec_t info = SSEXEC_ZERO ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    size_t scandirlen = info.scandir.len ;

    for (unsigned int i = 0 ; internal[i] ; i++) {

        char const *name = internal[i] ;
        size_t namelen = strlen(name) ;

        /* <scandir>/<name> -- the base write_min_resolve() wrote to. */
        char dir[scandirlen + 1 + namelen + 1] ;
        auto_strings(dir, info.scandir.s, "/", name) ;

        /* the split execute addon marks a resolve already converted: skip it
         * so a re-run (or a fresh scandir) never feeds a split cdb to the
         * monolithic reader. */
        char split[scandirlen + 1 + namelen + SS_RESOLVE_LEN + 1 + namelen + SS_ADDON_EXECUTE_SUFFIX_LEN + 1] ;
        auto_strings(split, dir, SS_RESOLVE, "/", name, SS_ADDON_EXECUTE_SUFFIX) ;
        if (!access(split, F_OK))
            continue ;

        /* the monolithic core sits at <dir>/.resolve/<name>. */
        char rdir[scandirlen + 1 + namelen + SS_RESOLVE_LEN + 1 + 1] ;
        auto_strings(rdir, dir, SS_RESOLVE, "/") ;

        migrate_scandir_resolve(rdir, dir, name) ;
    }

    ssexec_free(&info) ;
}

void migrate_0822(void)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.8.2.2 to: ", SS_VERSION) ;

    migrate_service_0822() ;
    migrate_scandir_0822() ;

    log_info("System successfully upgraded to version: ", SS_VERSION) ;
}
