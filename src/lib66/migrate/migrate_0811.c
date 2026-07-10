/*
 * migrate_0811.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>//strlen
#include <stdlib.h>//free,mkstemp
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>//fsync

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/cdb.h>
#include <oblibs/io.h>
#include <oblibs/files.h>
#include <oblibs/fd.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/module.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

#include <66/migrate_0811.h>
#include <66/migrate_0821.h>
#include <66/migrate.h>

static int service_resolve_read_cdb_0811(ocdb *c, resolve_service_t_0811 *res)
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
    if (!resolve_get_key_u32(c, "rversion", &res->rversion)) {
        free(wres) ;
        return (errno = EINVAL, 0)  ;
    }

    if (!resolve_get_key_u32(c, "name", &res->name) ||
        !resolve_get_key_u32(c, "description", &res->description) ||
        !resolve_get_key_u32(c, "version", &res->version) ||
        !resolve_get_key_u32(c, "type", &res->type) ||
        !resolve_get_key_u32(c, "notify", &res->notify) ||
        !resolve_get_key_u32(c, "maxdeath", &res->maxdeath) ||
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

    /* path configuration */
        !resolve_get_key_u32(c, "home", &res->path.home) ||
        !resolve_get_key_u32(c, "frontend", &res->path.frontend) ||
        !resolve_get_key_u32(c, "src_servicedir", &res->path.servicedir) ||

    /* dependencies */
        !resolve_get_key_u32(c, "depends", &res->dependencies.depends) ||
        !resolve_get_key_u32(c, "requiredby", &res->dependencies.requiredby) ||
        !resolve_get_key_u32(c, "optsdeps", &res->dependencies.optsdeps) ||
        !resolve_get_key_u32(c, "contents", &res->dependencies.contents) ||
        !resolve_get_key_u32(c, "provide", &res->dependencies.provide) ||
        !resolve_get_key_u32(c, "ndepends", &res->dependencies.ndepends) ||
        !resolve_get_key_u32(c, "nrequiredby", &res->dependencies.nrequiredby) ||
        !resolve_get_key_u32(c, "noptsdeps", &res->dependencies.noptsdeps) ||
        !resolve_get_key_u32(c, "ncontents", &res->dependencies.ncontents) ||
        !resolve_get_key_u32(c, "nprovide", &res->dependencies.nprovide) ||

    /* execute */
        !resolve_get_key_u32(c, "run", &res->execute.run.run) ||
        !resolve_get_key_u32(c, "run_user", &res->execute.run.run_user) ||
        !resolve_get_key_u32(c, "run_build", &res->execute.run.build) ||
        !resolve_get_key_u32(c, "run_runas", &res->execute.run.runas) ||
        !resolve_get_key_u32(c, "finish", &res->execute.finish.run) ||
        !resolve_get_key_u32(c, "finish_user", &res->execute.finish.run_user) ||
        !resolve_get_key_u32(c, "finish_build", &res->execute.finish.build) ||
        !resolve_get_key_u32(c, "finish_runas", &res->execute.finish.runas) ||
        !resolve_get_key_u32(c, "timeoutstart", &res->execute.timeout.start) ||
        !resolve_get_key_u32(c, "timeoutstop", &res->execute.timeout.stop) ||
        !resolve_get_key_u32(c, "down", &res->execute.down) ||
        !resolve_get_key_u32(c, "downsignal", &res->execute.downsignal) ||

    /* live */
        !resolve_get_key_u32(c, "livedir", &res->live.livedir) ||
        !resolve_get_key_u32(c, "status", &res->live.status) ||
        !resolve_get_key_u32(c, "live_servicedir", &res->live.servicedir) ||
        !resolve_get_key_u32(c, "scandir", &res->live.scandir) ||
        !resolve_get_key_u32(c, "statedir", &res->live.statedir) ||
        !resolve_get_key_u32(c, "eventdir", &res->live.eventdir) ||
        !resolve_get_key_u32(c, "notifdir", &res->live.notifdir) ||
        !resolve_get_key_u32(c, "supervisedir", &res->live.supervisedir) ||
        !resolve_get_key_u32(c, "fdholderdir", &res->live.fdholderdir) ||
        !resolve_get_key_u32(c, "oneshotddir", &res->live.oneshotddir) ||

    /* logger */
        !resolve_get_key_u32(c, "logname", &res->logger.name) ||
        !resolve_get_key_u32(c, "logbackup", &res->logger.backup) ||
        !resolve_get_key_u32(c, "logmaxsize", &res->logger.maxsize) ||
        !resolve_get_key_u32(c, "logtimestamp", &res->logger.timestamp) ||
        !resolve_get_key_u32(c, "logwant", &res->logger.want) ||
        !resolve_get_key_u32(c, "logrun", &res->logger.execute.run.run) ||
        !resolve_get_key_u32(c, "logrun_user", &res->logger.execute.run.run_user) ||
        !resolve_get_key_u32(c, "logrun_build", &res->logger.execute.run.build) ||
        !resolve_get_key_u32(c, "logrun_runas", &res->logger.execute.run.runas) ||
        !resolve_get_key_u32(c, "logtimeoutstart", &res->logger.execute.timeout.start) ||
        !resolve_get_key_u32(c, "logtimeoutstop", &res->logger.execute.timeout.stop) ||

    /* environment */
        !resolve_get_key_u32(c, "env", &res->environ.env) ||
        !resolve_get_key_u32(c, "envdir", &res->environ.envdir) ||
        !resolve_get_key_u32(c, "env_overwrite", &res->environ.env_overwrite) ||
        !resolve_get_key_u32(c, "importfile", &res->environ.importfile) ||
        !resolve_get_key_u32(c, "nimportfile", &res->environ.nimportfile) ||

    /* regex */
        !resolve_get_key_u32(c, "configure", &res->regex.configure) ||
        !resolve_get_key_u32(c, "directories", &res->regex.directories) ||
        !resolve_get_key_u32(c, "files", &res->regex.files) ||
        !resolve_get_key_u32(c, "infiles", &res->regex.infiles) ||
        !resolve_get_key_u32(c, "ndirectories", &res->regex.ndirectories) ||
        !resolve_get_key_u32(c, "nfiles", &res->regex.nfiles) ||
        !resolve_get_key_u32(c, "ninfiles", &res->regex.ninfiles) ||

    /* io */
        !resolve_get_key_u32(c, "stdintype", &res->io.fdin.type) ||
        !resolve_get_key_u32(c, "stdindest", &res->io.fdin.destination) ||
        !resolve_get_key_u32(c, "stdouttype", &res->io.fdout.type) ||
        !resolve_get_key_u32(c, "stdoutdest", &res->io.fdout.destination) ||
        !resolve_get_key_u32(c, "stderrtype", &res->io.fderr.type) ||
        !resolve_get_key_u32(c, "stderrdest", &res->io.fderr.destination)) {
            free(wres) ;
            return (errno = EINVAL, 0)  ;
    }

    free(wres) ;

    return 1 ;
}

static void add_version_0821(resolve_service_t_0821 *res)
{
    log_flow() ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    res->rversion = resolve_add_string(wres, SS_VERSION) ;
    free(wres) ;
}

static int service_resolve_write_cdb_0821(ocdbmaker *c, resolve_service_t_0821 *res)
{
    log_flow() ;

    add_version_0821(res) ;

    if (!ocdb_make_add(c, "sa", 2, res->sa.s, res->sa.len))
        return 0 ;

    if (!resolve_add_cdb_uint(c, "rversion", res->rversion) ||
        !resolve_add_cdb_uint(c, "name", res->name) ||
        !resolve_add_cdb_uint(c, "description", res->description) ||
        !resolve_add_cdb_uint(c, "version", res->version) ||
        !resolve_add_cdb_uint(c, "type", res->type) ||
        !resolve_add_cdb_uint(c, "notify", res->notify) ||
        !resolve_add_cdb_uint(c, "maxdeath", res->maxdeath) ||
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

        // live
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
        !resolve_add_cdb_uint(c, "logtimestamp", res->logger.timestamp) ||
        !resolve_add_cdb_uint(c, "logwant", res->logger.want) ||
        !resolve_add_cdb_uint(c, "logrun", res->logger.execute.run.run) ||
        !resolve_add_cdb_uint(c, "logrun_user", res->logger.execute.run.run_user) ||
        !resolve_add_cdb_uint(c, "logrun_build", res->logger.execute.run.build) ||
        !resolve_add_cdb_uint(c, "logrun_runas", res->logger.execute.run.runas) ||
        !resolve_add_cdb_uint(c, "logtimeoutstart", res->logger.execute.timeout.start) ||
        !resolve_add_cdb_uint(c, "logtimeoutstop", res->logger.execute.timeout.stop) ||

        // environ
        !resolve_add_cdb_uint(c, "env", res->environ.env) ||
        !resolve_add_cdb_uint(c, "envdir", res->environ.envdir) ||
        !resolve_add_cdb_uint(c, "env_overwrite", res->environ.env_overwrite) ||
        !resolve_add_cdb_uint(c, "importfile", res->environ.importfile) ||
        !resolve_add_cdb_uint(c, "nimportfile", res->environ.nimportfile) ||

        // regex
        !resolve_add_cdb_uint(c, "configure", res->regex.configure) ||
        !resolve_add_cdb_uint(c, "directories", res->regex.directories) ||
        !resolve_add_cdb_uint(c, "files", res->regex.files) ||
        !resolve_add_cdb_uint(c, "infiles", res->regex.infiles) ||
        !resolve_add_cdb_uint(c, "ndirectories", res->regex.ndirectories) ||
        !resolve_add_cdb_uint(c, "nfiles", res->regex.nfiles) ||
        !resolve_add_cdb_uint(c, "ninfiles", res->regex.ninfiles) ||

        // io
        !resolve_add_cdb_uint(c, "stdintype", res->io.fdin.type) ||
        !resolve_add_cdb_uint(c, "stdindest", res->io.fdin.destination) ||
        !resolve_add_cdb_uint(c, "stdouttype", res->io.fdout.type) ||
        !resolve_add_cdb_uint(c, "stdoutdest", res->io.fdout.destination) ||
        !resolve_add_cdb_uint(c, "stderrtype", res->io.fderr.type) ||
        !resolve_add_cdb_uint(c, "stderrdest", res->io.fderr.destination) ||

        // limit
        !resolve_add_cdb_uint64(c, "limitas", res->limit.limitas) ||
        !resolve_add_cdb_uint64(c, "limitcore", res->limit.limitcore) ||
        !resolve_add_cdb_uint64(c, "limitcpu", res->limit.limitcpu) ||
        !resolve_add_cdb_uint64(c, "limitdata", res->limit.limitdata) ||
        !resolve_add_cdb_uint64(c, "limitfsize", res->limit.limitfsize) ||
        !resolve_add_cdb_uint64(c, "limitlocks", res->limit.limitlocks) ||
        !resolve_add_cdb_uint64(c, "limitmemlock", res->limit.limitmemlock) ||
        !resolve_add_cdb_uint64(c, "limitmsgqueue", res->limit.limitmsgqueue) ||
        !resolve_add_cdb_uint64(c, "limitnice", res->limit.limitnice) ||
        !resolve_add_cdb_uint64(c, "limitnofile", res->limit.limitnofile) ||
        !resolve_add_cdb_uint64(c, "limitnproc", res->limit.limitnproc) ||
        !resolve_add_cdb_uint64(c, "limitrtprio", res->limit.limitrtprio) ||
        !resolve_add_cdb_uint64(c, "limitrttime", res->limit.limitrttime) ||
        !resolve_add_cdb_uint64(c, "limitsigpending", res->limit.limitsigpending) ||
        !resolve_add_cdb_uint64(c, "limitstack", res->limit.limitstack))
            return 0 ;

    return 1 ;
}

int migrate_write_frozen_0821(resolve_service_t_0821 *res, char const *base, char const *name)
{
    log_flow() ;

    int fd ;
    size_t baselen = strlen(base), namelen = strlen(name) ;
    ocdbmaker c = OCDBMAKER_ZERO ;

    char file[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    char tfile[5 + namelen + 8] ;

    auto_strings(file, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name, SS_RESOLVE, "/", name) ;
    auto_strings(tfile, "/tmp/", name, ":", "XXXXXX") ;

    fd = mkstemp(tfile) ;
    if (fd < 0 || !io_set_block(fd)) {
        log_warnusys("mkstemp: ", tfile) ;
        goto err_fd ;
    }

    if (!ocdb_make_start(&c, fd)) {
        log_warnusys("cdbmake_start") ;
        goto err ;
    }

    if (!service_resolve_write_cdb_0821(&c, res))
        goto err ;

    if (!ocdb_make_finish(&c) || fsync(fd) < 0) {
        log_warnusys("write to: ", tfile) ;
        goto err ;
    }

    close_fd(fd) ;

    if (!file_copy(tfile, file, 0600)) {
        log_warnusys("copy: ", tfile, " to ", file) ;
        goto err_fd ;
    }

    file_tryunlink(tfile) ;

    return 1 ;

    err:
        close_fd(fd) ;
    err_fd:
        file_tryunlink(tfile) ;
        return 0 ;
}

static void service_resolve_sanitize_0811(resolve_service_t_0821 *new, resolve_service_t_0811 *old)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, new) ;

    resolve_init(wres) ;

    // configuration
    new->name = old->name ? resolve_add_string(wres, old->sa.s + old->name) : 0 ;
    new->description = old->description ? resolve_add_string(wres, old->sa.s + old->description) : 0 ;
    new->version = old->version ? resolve_add_string(wres, old->sa.s + old->version) : 0 ;
    new->type = old->type ;
    new->notify = old->notify ;
    new->maxdeath = old->maxdeath ;
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

    // dependencies (conflict/nconflict are new in 0.8.2.x -> zero default)
    new->dependencies.depends = old->dependencies.depends ? resolve_add_string(wres, old->sa.s + old->dependencies.depends) : 0 ;
    new->dependencies.requiredby = old->dependencies.requiredby ? resolve_add_string(wres, old->sa.s + old->dependencies.requiredby) : 0 ;
    new->dependencies.optsdeps = old->dependencies.optsdeps ? resolve_add_string(wres, old->sa.s + old->dependencies.optsdeps) : 0 ;
    new->dependencies.contents = old->dependencies.contents ? resolve_add_string(wres, old->sa.s + old->dependencies.contents) : 0 ;
    new->dependencies.provide = old->dependencies.provide ? resolve_add_string(wres, old->sa.s + old->dependencies.provide) : 0 ;
    new->dependencies.ndepends = old->dependencies.ndepends ;
    new->dependencies.nrequiredby = old->dependencies.nrequiredby ;
    new->dependencies.noptsdeps = old->dependencies.noptsdeps ;
    new->dependencies.ncontents = old->dependencies.ncontents ;
    new->dependencies.nprovide = old->dependencies.nprovide ;

    // execute (blockprivileges..ncapsambient are new in 0.8.2.x -> zero default)
    new->execute.run.run = old->execute.run.run ? resolve_add_string(wres, old->sa.s + old->execute.run.run) : 0 ;
    new->execute.run.run_user = old->execute.run.run_user ? resolve_add_string(wres, old->sa.s + old->execute.run.run_user) : 0 ;
    new->execute.run.build = old->execute.run.build ? resolve_add_string(wres, old->sa.s + old->execute.run.build) : 0 ;
    new->execute.run.runas = old->execute.run.runas ? resolve_add_string(wres, old->sa.s + old->execute.run.runas) : 0 ;
    new->execute.finish.run = old->execute.finish.run ? resolve_add_string(wres, old->sa.s + old->execute.finish.run) : 0 ;
    new->execute.finish.run_user = old->execute.finish.run_user ? resolve_add_string(wres, old->sa.s + old->execute.finish.run_user) : 0 ;
    new->execute.finish.build = old->execute.finish.build ? resolve_add_string(wres, old->sa.s + old->execute.finish.build) : 0 ;
    new->execute.finish.runas = old->execute.finish.runas ? resolve_add_string(wres, old->sa.s + old->execute.finish.runas) : 0 ;
    new->execute.timeout.start = old->execute.timeout.start ;
    new->execute.timeout.stop = old->execute.timeout.stop ;
    new->execute.down = old->execute.down ;
    new->execute.downsignal = old->execute.downsignal ;

    // live
    new->live.livedir = old->live.livedir ? resolve_add_string(wres, old->sa.s + old->live.livedir) : 0 ;
    new->live.status = old->live.status ? resolve_add_string(wres, old->sa.s + old->live.status) : 0 ;
    new->live.servicedir = old->live.servicedir ? resolve_add_string(wres, old->sa.s + old->live.servicedir) : 0 ;
    new->live.scandir = old->live.scandir ? resolve_add_string(wres, old->sa.s + old->live.scandir) : 0 ;
    new->live.statedir = old->live.statedir ? resolve_add_string(wres, old->sa.s + old->live.statedir) : 0 ;
    new->live.eventdir = old->live.eventdir ? resolve_add_string(wres, old->sa.s + old->live.eventdir) : 0 ;
    new->live.notifdir = old->live.notifdir ? resolve_add_string(wres, old->sa.s + old->live.notifdir) : 0 ;
    new->live.supervisedir = old->live.supervisedir ? resolve_add_string(wres, old->sa.s + old->live.supervisedir) : 0 ;
    new->live.fdholderdir = old->live.fdholderdir ? resolve_add_string(wres, old->sa.s + old->live.fdholderdir) : 0 ;
    new->live.oneshotddir = old->live.oneshotddir ? resolve_add_string(wres, old->sa.s + old->live.oneshotddir) : 0 ;

    // logger (0.8.2.x drops the trailing separate timeout field; execute.timeout is the used one)
    new->logger.name = old->logger.name ? resolve_add_string(wres, old->sa.s + old->logger.name) : 0 ;
    new->logger.backup = old->logger.backup ;
    new->logger.maxsize = old->logger.maxsize ;
    new->logger.timestamp = old->logger.timestamp ;
    new->logger.want = old->logger.want ;
    new->logger.execute.run.run = old->logger.execute.run.run ? resolve_add_string(wres, old->sa.s + old->logger.execute.run.run) : 0 ;
    new->logger.execute.run.run_user = old->logger.execute.run.run_user ? resolve_add_string(wres, old->sa.s + old->logger.execute.run.run_user) : 0 ;
    new->logger.execute.run.build = old->logger.execute.run.build ? resolve_add_string(wres, old->sa.s + old->logger.execute.run.build) : 0 ;
    new->logger.execute.run.runas = old->logger.execute.run.runas ? resolve_add_string(wres, old->sa.s + old->logger.execute.run.runas) : 0 ;
    new->logger.execute.timeout.start = old->logger.execute.timeout.start ;
    new->logger.execute.timeout.stop = old->logger.execute.timeout.stop ;

    // environ
    new->environ.env = old->environ.env ? resolve_add_string(wres, old->sa.s + old->environ.env) : 0 ;
    new->environ.envdir = old->environ.envdir ? resolve_add_string(wres, old->sa.s + old->environ.envdir) : 0 ;
    new->environ.env_overwrite = old->environ.env_overwrite ;
    new->environ.importfile = old->environ.importfile ? resolve_add_string(wres, old->sa.s + old->environ.importfile) : 0 ;
    new->environ.nimportfile = old->environ.nimportfile ;

    // regex
    new->regex.configure = old->regex.configure ? resolve_add_string(wres, old->sa.s + old->regex.configure) : 0 ;
    new->regex.directories = old->regex.directories ? resolve_add_string(wres, old->sa.s + old->regex.directories) : 0 ;
    new->regex.files = old->regex.files ? resolve_add_string(wres, old->sa.s + old->regex.files) : 0 ;
    new->regex.infiles = old->regex.infiles ? resolve_add_string(wres, old->sa.s + old->regex.infiles) : 0 ;
    new->regex.ndirectories = old->regex.ndirectories ;
    new->regex.nfiles = old->regex.nfiles ;
    new->regex.ninfiles = old->regex.ninfiles ;

    // io
    new->io.fdin.type = old->io.fdin.type ;
    new->io.fdin.destination = old->io.fdin.destination ? resolve_add_string(wres, old->sa.s + old->io.fdin.destination) : 0 ;
    new->io.fdout.type = old->io.fdout.type ;
    new->io.fdout.destination = old->io.fdout.destination ? resolve_add_string(wres, old->sa.s + old->io.fdout.destination) : 0 ;
    new->io.fderr.type = old->io.fderr.type ;
    new->io.fderr.destination = old->io.fderr.destination ? resolve_add_string(wres, old->sa.s + old->io.fderr.destination) : 0 ;

    free(wres) ;
}

static void migrate_resolve(ssexec_t *info, const char *path, const char *name)
{
    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_t_0811 res = RESOLVE_SERVICE_ZERO_0811 ;
    resolve_service_t_0821 new = RESOLVE_SERVICE_ZERO_0821 ;

    if (resolve_open_cdb(&fd, &c, path, name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "open resolve file of service: ", name) ;

    if (!service_resolve_read_cdb_0811(&c, &res))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of service: ", name) ;

    service_resolve_sanitize_0811(&new, &res) ;

    if (!migrate_write_frozen_0821(&new, info->base.s, name))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

    strbuf_free(&new.sa) ;
}

static void migrate_service_0811(void)
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

void migrate_0811(void)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.8.1.1 to: ", SS_VERSION) ;

    migrate_service_0811() ;

    log_info("System successfully upgraded to version: ", SS_VERSION) ;
}
