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
#include <oblibs/files.h>
#include <oblibs/account.h>
#include <oblibs/directory.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/module.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

#include <66/migrate_0821.h>
#include <66/migrate.h>

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
        !resolve_get_key_u32(c, "conflict", &res->dependencies.conflict) ||
        !resolve_get_key_u32(c, "ndepends", &res->dependencies.ndepends) ||
        !resolve_get_key_u32(c, "nrequiredby", &res->dependencies.nrequiredby) ||
        !resolve_get_key_u32(c, "noptsdeps", &res->dependencies.noptsdeps) ||
        !resolve_get_key_u32(c, "ncontents", &res->dependencies.ncontents) ||
        !resolve_get_key_u32(c, "nprovide", &res->dependencies.nprovide) ||
        !resolve_get_key_u32(c, "nconflict", &res->dependencies.nconflict) ||

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
        !resolve_get_key_u32(c, "blockprivileges", &res->execute.blockprivileges) ||
        !resolve_get_key_u32(c, "umask", &res->execute.umask) ||
        !resolve_get_key_u32(c, "want_umask", &res->execute.want_umask) ||
        !resolve_get_key_u32(c, "nice", &res->execute.nice) ||
        !resolve_get_key_u32(c, "want_nice", &res->execute.want_nice) ||
        !resolve_get_key_u32(c, "chdir", &res->execute.chdir) ||
        !resolve_get_key_u32(c, "capsbound", &res->execute.capsbound) ||
        !resolve_get_key_u32(c, "capsambient", &res->execute.capsambient) ||
        !resolve_get_key_u32(c, "ncapsbound", &res->execute.ncapsbound) ||
        !resolve_get_key_u32(c, "ncapsambient", &res->execute.ncapsambient) ||

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
        !resolve_get_key_u32(c, "stderrdest", &res->io.fderr.destination) ||

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
    /* scandir: oneshot/module entries are hidden from 66-scandir with a leading
     * dot on the name (see compute_scan_dir). The pre-split 0.8.x layout stored no
     * such dot, so re-derive it from the type instead of copying verbatim --
     * otherwise 66-scandir supervises a oneshot and crash-loops it at boot. */
    if (old->live.scandir) {
        char const *oldsd = old->sa.s + old->live.scandir ;
        char const *slash = strrchr(oldsd, '/') ;
        size_t prefixlen = slash ? (size_t)(slash + 1 - oldsd) : 0 ;
        char const *entry = oldsd + prefixlen ;
        if (*entry == '.') entry++ ; // bare name -- idempotent if already dotted
        char const *dot = new->type == E_PARSER_TYPE_CLASSIC ? "" : "." ;
        char sd[prefixlen + 1 + strlen(entry) + 1] ;
        memcpy(sd, oldsd, prefixlen) ;
        auto_strings(sd + prefixlen, dot, entry) ;
        new->live.scandir = resolve_add_string(wres, sd) ;
    } else new->live.scandir = 0 ;
    new->live.statedir = old->live.statedir ? resolve_add_string(wres, old->sa.s + old->live.statedir) : 0 ;
    new->live.eventdir = old->live.eventdir ? resolve_add_string(wres, old->sa.s + old->live.eventdir) : 0 ;
    new->live.supervisedir = old->live.supervisedir ? resolve_add_string(wres, old->sa.s + old->live.supervisedir) : 0 ;
    new->live.oneshotddir = old->live.oneshotddir ? resolve_add_string(wres, old->sa.s + old->live.oneshotddir) : 0 ;
    if (old->live.oneshotddir) {
        // derive <scandir>/fdholderd and <scandir>/eventd from the sibling
        // <scandir>/oneshotd, so a renamed SS_FDHOLDER/SS_EVENTD is picked up
        char const *osd = old->sa.s + old->live.oneshotddir ;
        size_t rootlen = strlen(osd) - SS_ONESHOTD_LEN ;

        char fh[rootlen + SS_FDHOLDER_LEN + 1] ;
        memcpy(fh, osd, rootlen) ;
        auto_strings(fh + rootlen, SS_FDHOLDER) ;
        new->live.fdholderdir = resolve_add_string(wres, fh) ;

        char ev[rootlen + SS_EVENTD_LEN + 1] ;
        memcpy(ev, osd, rootlen) ;
        auto_strings(ev + rootlen, SS_EVENTD) ;
        new->live.eventddir = resolve_add_string(wres, ev) ;
    } else {
        new->live.fdholderdir = 0 ;
        new->live.eventddir = 0 ;
    }

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

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

static void compute_log_script(resolve_service_t *log, resolve_service_addon_execute_t *logexec, resolve_service_addon_io_t *io, resolve_service_addon_logger_t *lg)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, logexec) ;

    int build = !strcmp(lg->sa.s + lg->execute.run.build, "custom") ? E_PARSER_BUILD_CUSTOM : E_PARSER_BUILD_AUTO ;

    char *pmax = 0 ;
    char *pback = 0 ;
    char max[U32_FMT] ;
    char back[U32_FMT] ;
    char *timestamp = 0 ;
    int itimestamp = SS_LOGGER_TIMESTAMP ;
    char *logrunner = lg->execute.run.runas ? lg->sa.s + lg->execute.run.runas : SS_LOGGER_RUNNER ;

    logexec->run.runas = resolve_add_string(wres, logrunner) ;

    // timestamp
    uint32_t ts = lg->timestamp == E_PARSER_TIME_ENDOFKEY ? (uint32_t)itimestamp : lg->timestamp ;
    timestamp = ts == E_PARSER_TIME_NONE ? "" : ts == E_PARSER_TIME_ISO ? "T" : "t" ;

    /** backup */
    if (lg->backup) {

        back[u32_fmt(back,lg->backup)] = 0 ;
        pback = back ;

    }

    /** file size */
    if (lg->maxsize) {

        max[u32_fmt(max,lg->maxsize)] = 0 ;
        pmax = max ;

    }

    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;

    {
        /** run scripts */
        char run[strlen(shebang) + strlen(SS_EXTLIBEXECPREFIX) + 17 + strlen(log->sa.s + log->name) + 1 + 1] ;

        auto_strings(run, \
                    shebang, \
                    SS_EXTLIBEXECPREFIX "66-execute start ", \
                    log->sa.s + log->name, "\n") ;

        logexec->run.run = resolve_add_string(wres, run) ;

    }

    {
        if (!build) {
            /** run.user script */
            char run[SS_MAX_PATH_LEN + 1] ;

            auto_strings(run, shebang) ;

            auto_strings(run + FAKELEN, SS_BINPREFIX "66-log ") ;

            if (SS_LOGGER_NOTIFY)
                auto_strings(run + FAKELEN, "-d3 ") ;

            auto_strings(run + FAKELEN, "n", pback, " ") ;

            if (timestamp[0])
                auto_strings(run + FAKELEN, timestamp, " ") ;

            auto_strings(run + FAKELEN, "s", pmax, " ", io->sa.s + io->fdout.destination, "\n") ;

            logexec->run.run_user = resolve_add_string(wres, run) ;

        } else {

            char run[strlen(lg->sa.s + lg->execute.run.run_user) + 2] ;
            auto_strings(run, lg->sa.s + lg->execute.run.run_user, "\n") ;

            logexec->run.run_user = resolve_add_string(wres, run) ;
        }
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

    if (new.islog && new.type == E_PARSER_TYPE_CLASSIC) {

        resolve_wrapper_t_ref lgw = resolve_set_struct(DATA_SERVICE_LOGGER, &logger) ;
        resolve_init(lgw) ;
        logger.backup = res.logger.backup ;
        logger.maxsize = res.logger.maxsize ;
        logger.timestamp = res.logger.timestamp ;
        logger.execute.run.build = res.logger.execute.run.build ? resolve_add_string(lgw, res.sa.s + res.logger.execute.run.build) : 0 ;
        logger.execute.run.runas = res.logger.execute.run.runas ? resolve_add_string(lgw, res.sa.s + res.logger.execute.run.runas) : 0 ;
        logger.execute.run.run_user = res.logger.execute.run.run_user ? resolve_add_string(lgw, res.sa.s + res.logger.execute.run.run_user) : 0 ;
        free(lgw) ;

        compute_log_script(&new, &execute, &io, &logger) ;
    }

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

        /** A provide alias is a relative symlink to another service; it owns no
         * resolve file of its own and is migrated under its target's real name.
         * Skip it here -- otherwise opening <alias>/.resolve/<alias> fails with
         * ENOENT and aborts the whole migration. */
        char lnk[info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        char rname[SS_MAX_SERVICE_NAME + 1] ;
        auto_strings(lnk, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;
        auto_strings(rname, name) ;

        if (!service_resolve_symlink(info.base.s, lnk, rname))
            log_dieusys(LOG_EXIT_SYS, "resolve symlink of service: ", name) ;

        if (strcmp(rname, name))
            continue ;

        auto_strings(path + len, name, SS_RESOLVE, "/") ;

        migrate_resolve(&info, path, name) ;
    }

    ssexec_free(&info) ;
}

/** The live scandir is the one 0.8.2.2 booted: s6-svscan supervises it and its
 * run scripts still exec the binaries and options of that release, which this
 * one no longer provides. Nothing rewrites them before the next boot, and the
 * supervisor does relaunch these daemons -- 66-shutdownd relies on it to run
 * the final stage of a shutdown. What follows writes what a 0.9.0.0 scandir
 * carries, frozen here: it belongs to this migration step and dies with it.
 *
 * A failure only costs the rewrite: the state on disk is already migrated at
 * this point, so warn and carry on rather than abort and leave the system
 * halfway. */

#define MIGRATE_SCANDIR_HINT " -- you may need to force the reboot using 66 reboot -f"

static int migrate_scandir_run(char const *dir, char const *run, size_t runlen)
{
    log_flow() ;

    char dst[strlen(dir) + 5] ;
    auto_strings(dst, dir, "/run") ;

    log_trace("write run file: ", dst) ;

    if (!file_write(dst, run, runlen) || chmod(dst, 0755) < 0) {
        log_warnusys("write run file: ", dst, MIGRATE_SCANDIR_HINT) ;
        return 0 ;
    }

    return 1 ;
}

static void migrate_scandir_resolve(char const *dir, char const *name, uint32_t notify)
{
    log_flow() ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_wrapper_t_ref wex = resolve_set_struct(DATA_SERVICE_EXECUTE, &ex) ;

    resolve_init(wres) ;
    resolve_init(wex) ;

    res.name = resolve_add_string(wres, name) ;
    res.type = E_PARSER_TYPE_CLASSIC ;
    res.has_execute = 1 ;
    ex.notify = notify ;

    char rdir[strlen(dir) + SS_RESOLVE_LEN + 1] ;
    auto_strings(rdir, dir, SS_RESOLVE) ;

    char aname[strlen(name) + SS_ADDON_EXECUTE_SUFFIX_LEN + 1] ;
    auto_strings(aname, name, SS_ADDON_EXECUTE_SUFFIX) ;

    // resolve_write_at does not create the .resolve directory
    if (!dir_create_parent(rdir, 0755)
        || !resolve_write_at(wres, dir, name)
        || !resolve_write_at(wex, dir, aname))
            log_warnusys("write resolve file of service: ", name, MIGRATE_SCANDIR_HINT) ;

    resolve_free(wres) ;
    resolve_free(wex) ;
}

static void migrate_scandir_notification(char const *dir)
{
    log_flow() ;

    char dst[strlen(dir) + 1 + SS_NOTIFICATION_LEN + 1] ;
    auto_strings(dst, dir, "/", SS_NOTIFICATION) ;

    if (!file_write(dst, "3\n", 2))
        log_warnusys("write file: ", dst, MIGRATE_SCANDIR_HINT) ;
}

/** The daemon that shuts the system down keeps running from memory until the
 * reboot: it is the 0.8.2.2 one, and it is the right one -- it drives an s6
 * scandir this release cannot drive. Its last act is to leave a stage4 file
 * and exit, for the supervisor to relaunch a process that unmounts and calls
 * 66-hpr. That relaunch is what this run file must serve, and 0.8.2.2 writes
 * stage4 as an executable script, where this release writes a one byte marker
 * its own binary reads. So run the script when it is there -- exactly what the
 * 0.8.2.2 binary did on startup -- and fall back to the daemon otherwise. The
 * next boot writes a plain run file: this one only has to survive until then. */
static void migrate_scandir_shutdownd(char const *live, char const *dir, unsigned int container, unsigned int catch_log)
{
    log_flow() ;

    strbuf b = STRBUF_ZERO ;

    if (!auto_strbuf(&b,
        "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
        SS_EXECLINE_BINPREFIX "foreground { " SS_EXECLINE_BINPREFIX "tryexec { ./stage4 } }\n" \
        SS_LIBEXECPREFIX "66-shutdownd -l ", live, " -g 3000")
        || (container && !auto_strbuf(&b, " -B"))
        || (!catch_log && !auto_strbuf(&b, " -c"))
        || !auto_strbuf(&b, "\n"))
            log_die_nomem("strbuf") ;

    if (migrate_scandir_run(dir, b.s, b.len))
        migrate_scandir_resolve(dir, "66-shutdownd", 0) ;

    strbuf_free(&b) ;
}

static void migrate_scandir_log(char const *dir, char const *logdir, char const *user, unsigned int container)
{
    log_flow() ;

    strbuf b = STRBUF_ZERO ;

    if (!auto_strbuf(&b,
        "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n",
        container ? SS_EXECLINE_BINPREFIX "fdmove -c 1 2\n" : SS_EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n",
        SS_EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
        SS_BINPREFIX "execl-runas ", user,
        "\n" SS_BINPREFIX "66-log -bpd3 --")
        || (SS_LOGGER_TIMESTAMP < E_PARSER_TIME_NONE
            && !auto_strbuf(&b, SS_LOGGER_TIMESTAMP == E_PARSER_TIME_ISO ? " T " : " t "))
        || !auto_strbuf(&b, logdir, "\n"))
            log_die_nomem("strbuf") ;

    if (migrate_scandir_run(dir, b.s, b.len)) {

        migrate_scandir_notification(dir) ;
        migrate_scandir_resolve(dir, "scandir-log", 3) ;
    }

    strbuf_free(&b) ;
}

static void migrate_scandir_socket(char const *dir, char const *name, char const *bin, char const *operand)
{
    log_flow() ;

    strbuf b = STRBUF_ZERO ;
    char verbo[U32_FMT] ;

    verbo[u32_fmt(verbo, VERBOSITY)] = 0 ;

    if (!auto_strbuf(&b,
        "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
        "fdmove -c 2 1\n" \
        SS_LIBEXECPREFIX, bin, " -v", verbo, " -d 3 -- ", operand, "\n"))
            log_die_nomem("strbuf") ;

    if (migrate_scandir_run(dir, b.s, b.len)) {

        migrate_scandir_notification(dir) ;
        migrate_scandir_resolve(dir, name, 3) ;
    }

    strbuf_free(&b) ;
}

static void migrate_scandir_0822(void)
{
    log_flow() ;

    // the internal services of a 0.8.2.2 scandir, by the name it gave them
    static char const *const socketd[][3] = {
        { "oneshotd", "66-oneshotd", "s" },
        { "fdholder", "66-fdholderd", "s" },
        { 0, 0, 0 }
    } ;

    ssexec_t info = SSEXEC_ZERO ;
    struct stat st ;
    strbuf user = STRBUF_ZERO ;
    unsigned int container = 0, catch_log = 0 ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    char const *live = info.live.s ;
    char const *scandir = info.scandir.s ;
    size_t livelen = info.live.len, scandirlen = info.scandir.len ;

    // no live scandir: a fresh /run, the boot creates it with this release
    if (!access(scandir, F_OK)) {

        char logdir[scandirlen + 1 + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1] ;
        auto_strings(logdir, scandir, "/" SS_SCANDIR SS_LOG_SUFFIX) ;

        catch_log = !access(logdir, F_OK) ;

        {
            char tmp[livelen + SS_BOOT_CONTAINER_DIR_LEN + 1 + info.ownerlen + 1] ;
            auto_strings(tmp, live, SS_BOOT_CONTAINER_DIR, "/", info.ownerstr) ;

            container = !access(tmp, F_OK) ;
        }

        {
            char shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 1] ;
            auto_strings(shut, scandir, "/", "66-shutdownd") ;

            if (!access(shut, F_OK))
                migrate_scandir_shutdownd(live, shut, container, catch_log) ;
        }

        if (catch_log) {

            char path[livelen + SS_LOG_LEN + 1 + info.ownerlen + 1] ;
            auto_strings(path, live, SS_LOG, "/", info.ownerstr) ;

            // the log directory belongs to the user the catch-all logger runs as
            if (stat(path, &st) < 0 || !get_namebyuid(st.st_uid, &user) || !strbuf_uncounted(&user))
                log_warnusys("get the owner of: ", path, " -- keeping the run file of the catch-all logger untouched") ;

            else
                migrate_scandir_log(logdir, path, user.s, container) ;
        }

        for (unsigned int i = 0 ; socketd[i][0] ; i++) {

            char tmp[scandirlen + 1 + strlen(socketd[i][0]) + 1] ;
            auto_strings(tmp, scandir, "/", socketd[i][0]) ;

            if (!access(tmp, F_OK))
                migrate_scandir_socket(tmp, socketd[i][0], socketd[i][1], socketd[i][2]) ;
        }
    }

    strbuf_free(&user) ;
    ssexec_free(&info) ;
}

static void migrate_log_owner_0822(void)
{
    log_flow() ;

    uid_t uid ;
    gid_t gid ;

    if (getuid())
        return ;

    if (access(SS_LOGGER_SYSDIR, F_OK) < 0) {
        log_warnusys("find logger directory: '" SS_LOGGER_SYSDIR "' -- ignoring it") ;
        return ;
    }

    if (!youruid(&uid, SS_LOGGER_RUNNER))
        log_dieusys(LOG_EXIT_SYS, "get uid of account: ", SS_LOGGER_RUNNER) ;

    if (!yourgid(&gid, uid))
        log_dieusys(LOG_EXIT_SYS, "get gid") ;

    if (chown(SS_LOGGER_SYSDIR, uid, gid) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", SS_LOGGER_SYSDIR) ;
}

void migrate_0822(void)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.8.2.2 to: ", SS_VERSION) ;

    migrate_log_owner_0822() ;
    migrate_service_0822() ;
    migrate_scandir_0822() ;

    log_info("System successfully upgraded to version: ", SS_VERSION) ;
}
