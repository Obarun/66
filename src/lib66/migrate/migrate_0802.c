/*
 * migrate_0802.c
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

#include <66/migrate_0802.h>
#include <66/migrate.h>

static void service_resolve_sanitize_0802(resolve_service_t *new, resolve_service_addon_environ_t *e, resolve_service_t_0802 *old)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, new) ;

    resolve_init(wres) ;

    // configuration
    new->name = resolve_add_string(wres, old->sa.s + old->name) ;
    new->description = old->description ? resolve_add_string(wres, old->sa.s + old->description) : 0 ;
    new->version = old->version ? resolve_add_string(wres, old->sa.s + old->version) : 0 ;
    new->type = old->type ;
    new->notify = old->notify ;
    new->maxdeath = old->maxdeath ;
    new->earlier = old->earlier ;
    new->copyfrom = old->hiercopy ? resolve_add_string(wres, old->sa.s + old->hiercopy) : 0 ;
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

    // dependencies
    new->dependencies.depends = old->dependencies.depends ? resolve_add_string(wres, old->sa.s + old->dependencies.depends) : 0 ;
    new->dependencies.requiredby = old->dependencies.requiredby ? resolve_add_string(wres, old->sa.s + old->dependencies.requiredby) : 0 ;
    new->dependencies.optsdeps = old->dependencies.optsdeps ? resolve_add_string(wres, old->sa.s + old->dependencies.optsdeps) : 0 ;
    new->dependencies.contents = old->dependencies.contents ? resolve_add_string(wres, old->sa.s + old->dependencies.contents) : 0 ;
    new->dependencies.ndepends = old->dependencies.ndepends ;
    new->dependencies.nrequiredby = old->dependencies.nrequiredby ;
    new->dependencies.noptsdeps = old->dependencies.noptsdeps ;
    new->dependencies.ncontents = old->dependencies.ncontents ;

    // execute
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

    // logger
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
    new->logger.execute.down = old->logger.execute.down ;
    new->logger.execute.downsignal = old->logger.execute.downsignal ;

    // environment -> autonomous addon (routed out of the core; 0802 had no importfile)
    new->has_environ = old->environ.env ? 1 : 0 ;
    if (new->has_environ) {
        resolve_wrapper_t_ref ewres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;
        resolve_init(ewres) ;
        e->env = old->environ.env ? resolve_add_string(ewres, old->sa.s + old->environ.env) : 0 ;
        e->envdir = old->environ.envdir ? resolve_add_string(ewres, old->sa.s + old->environ.envdir) : 0 ;
        e->env_overwrite = old->environ.env_overwrite ;
        e->importfile = 0 ;
        e->nimportfile = 0 ;
        free(ewres) ;
    }

    // regex
    new->regex.configure = old->regex.configure ? resolve_add_string(wres, old->sa.s + old->regex.configure) : 0 ;
    new->regex.directories = old->regex.directories ? resolve_add_string(wres, old->sa.s + old->regex.directories) : 0 ;
    new->regex.files = old->regex.files ? resolve_add_string(wres, old->sa.s + old->regex.files) : 0 ;
    new->regex.infiles = old->regex.infiles ? resolve_add_string(wres, old->sa.s + old->regex.infiles) : 0 ;
    new->regex.ndirectories = old->regex.ndirectories ;
    new->regex.nfiles = old->regex.nfiles ;
    new->regex.ninfiles = old->regex.ninfiles ;

    // IO
    new->io.fdin.type = old->io.fdin.type ;
    new->io.fdin.destination = old->io.fdin.destination ? resolve_add_string(wres, old->sa.s + old->io.fdin.destination) : 0 ;
    new->io.fdout.type = old->io.fdout.type ;
    new->io.fdout.destination = old->io.fdout.destination ? resolve_add_string(wres, old->sa.s + old->io.fdout.destination) : 0 ;
    new->io.fderr.type = old->io.fderr.type ;
    new->io.fderr.destination = old->io.fderr.destination ? resolve_add_string(wres, old->sa.s + old->io.fderr.destination) : 0 ;

    free(wres) ;
}

static int service_resolve_read_cdb_0802(ocdb *c, resolve_service_t_0802 *res)
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
        !resolve_get_key(c, "hiercopy", &res->hiercopy) ||
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
            free(wres) ;
            return (errno = EINVAL, 0)  ;
    }

    free(wres) ;

    return 1 ;
}

static void migrate_resolve(ssexec_t *info, const char *path, const char *name)
{
    int fd ;
    ocdb c = OCDB_ZERO ;
    resolve_service_t_0802 res = RESOLVE_SERVICE_ZERO_0802 ;
    resolve_service_t new = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &new) ;

    if (resolve_open_cdb(&fd, &c, path, name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "open resolve file of service: ", name) ;

    if (!service_resolve_read_cdb_0802(&c, &res))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of service: ", name) ;

    resolve_service_addon_environ_t environ = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    service_resolve_sanitize_0802(&new, &environ, &res) ;

    migrate_ensure_log_owner(&new) ;

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

    resolve_free(wres) ;
}

static void migrate_service_0802(void)
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

void migrate_0802(void)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.8.0.2 to: ", SS_VERSION) ;

    migrate_service_0802() ;

    log_info("System successfully upgraded to version: ", SS_VERSION) ;
}