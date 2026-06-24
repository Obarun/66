/*
 * ssexec_resolve.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/info.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

/* One row per cdb key, in the exact order and with the exact names written by
 * service_resolve_write_cdb.c -- the key is what lives in the resolve file and
 * is what -f selects (and what a future modify-by-field would target). */

static info_field_t const fields[] = {
    { "name",            INFO_FIELD_STR, offsetof(resolve_service_t, name) },
    { "description",     INFO_FIELD_STR, offsetof(resolve_service_t, description) },
    { "version",         INFO_FIELD_STR, offsetof(resolve_service_t, version) },
    { "type",            INFO_FIELD_U32, offsetof(resolve_service_t, type) },
    { "notify",          INFO_FIELD_U32, offsetof(resolve_service_t, notify) },
    { "maxdeath",        INFO_FIELD_U32, offsetof(resolve_service_t, maxdeath) },
    { "earlier",         INFO_FIELD_U32, offsetof(resolve_service_t, earlier) },
    { "copyfrom",        INFO_FIELD_STR, offsetof(resolve_service_t, copyfrom) },
    { "intree",          INFO_FIELD_STR, offsetof(resolve_service_t, intree) },
    { "ownerstr",        INFO_FIELD_STR, offsetof(resolve_service_t, ownerstr) },
    { "owner",           INFO_FIELD_U32, offsetof(resolve_service_t, owner) },
    { "treename",        INFO_FIELD_STR, offsetof(resolve_service_t, treename) },
    { "user",            INFO_FIELD_STR, offsetof(resolve_service_t, user) },
    { "inns",            INFO_FIELD_STR, offsetof(resolve_service_t, inns) },
    { "enabled",         INFO_FIELD_U32, offsetof(resolve_service_t, enabled) },
    { "islog",           INFO_FIELD_U32, offsetof(resolve_service_t, islog) },

    { "home",            INFO_FIELD_STR, offsetof(resolve_service_t, path.home) },
    { "frontend",        INFO_FIELD_STR, offsetof(resolve_service_t, path.frontend) },
    { "src_servicedir",  INFO_FIELD_STR, offsetof(resolve_service_t, path.servicedir) },

    { "depends",         INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.depends) },
    { "requiredby",      INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.requiredby) },
    { "optsdeps",        INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.optsdeps) },
    { "contents",        INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.contents) },
    { "provide",         INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.provide) },
    { "conflict",        INFO_FIELD_STR, offsetof(resolve_service_t, dependencies.conflict) },
    { "ndepends",        INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.ndepends) },
    { "nrequiredby",     INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.nrequiredby) },
    { "noptsdeps",       INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.noptsdeps) },
    { "ncontents",       INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.ncontents) },
    { "nprovide",        INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.nprovide) },
    { "nconflict",       INFO_FIELD_U32, offsetof(resolve_service_t, dependencies.nconflict) },

    { "run",             INFO_FIELD_STR, offsetof(resolve_service_t, execute.run.run) },
    { "run_user",        INFO_FIELD_STR, offsetof(resolve_service_t, execute.run.run_user) },
    { "run_build",       INFO_FIELD_STR, offsetof(resolve_service_t, execute.run.build) },
    { "run_runas",       INFO_FIELD_STR, offsetof(resolve_service_t, execute.run.runas) },
    { "finish",          INFO_FIELD_STR, offsetof(resolve_service_t, execute.finish.run) },
    { "finish_user",     INFO_FIELD_STR, offsetof(resolve_service_t, execute.finish.run_user) },
    { "finish_build",    INFO_FIELD_STR, offsetof(resolve_service_t, execute.finish.build) },
    { "finish_runas",    INFO_FIELD_STR, offsetof(resolve_service_t, execute.finish.runas) },
    { "timeoutstart",    INFO_FIELD_U32, offsetof(resolve_service_t, execute.timeout.start) },
    { "timeoutstop",     INFO_FIELD_U32, offsetof(resolve_service_t, execute.timeout.stop) },
    { "down",            INFO_FIELD_U32, offsetof(resolve_service_t, execute.down) },
    { "downsignal",      INFO_FIELD_U32, offsetof(resolve_service_t, execute.downsignal) },
    { "blockprivileges", INFO_FIELD_U32, offsetof(resolve_service_t, execute.blockprivileges) },
    { "umask",           INFO_FIELD_U32, offsetof(resolve_service_t, execute.umask) },
    { "want_umask",      INFO_FIELD_U32, offsetof(resolve_service_t, execute.want_umask) },
    { "nice",            INFO_FIELD_U32, offsetof(resolve_service_t, execute.nice) },
    { "want_nice",       INFO_FIELD_U32, offsetof(resolve_service_t, execute.want_nice) },
    { "chdir",           INFO_FIELD_STR, offsetof(resolve_service_t, execute.chdir) },
    { "capsbound",       INFO_FIELD_STR, offsetof(resolve_service_t, execute.capsbound) },
    { "capsambient",     INFO_FIELD_STR, offsetof(resolve_service_t, execute.capsambient) },
    { "ncapsbound",      INFO_FIELD_U32, offsetof(resolve_service_t, execute.ncapsbound) },
    { "ncapsambient",    INFO_FIELD_U32, offsetof(resolve_service_t, execute.ncapsambient) },

    { "livedir",         INFO_FIELD_STR, offsetof(resolve_service_t, live.livedir) },
    { "status",          INFO_FIELD_STR, offsetof(resolve_service_t, live.status) },
    { "live_servicedir", INFO_FIELD_STR, offsetof(resolve_service_t, live.servicedir) },
    { "scandir",         INFO_FIELD_STR, offsetof(resolve_service_t, live.scandir) },
    { "statedir",        INFO_FIELD_STR, offsetof(resolve_service_t, live.statedir) },
    { "eventdir",        INFO_FIELD_STR, offsetof(resolve_service_t, live.eventdir) },
    { "notifdir",        INFO_FIELD_STR, offsetof(resolve_service_t, live.notifdir) },
    { "supervisedir",    INFO_FIELD_STR, offsetof(resolve_service_t, live.supervisedir) },
    { "fdholderdir",     INFO_FIELD_STR, offsetof(resolve_service_t, live.fdholderdir) },
    { "oneshotddir",     INFO_FIELD_STR, offsetof(resolve_service_t, live.oneshotddir) },

    { "logname",         INFO_FIELD_STR, offsetof(resolve_service_t, logger.name) },
    { "logbackup",       INFO_FIELD_U32, offsetof(resolve_service_t, logger.backup) },
    { "logmaxsize",      INFO_FIELD_U32, offsetof(resolve_service_t, logger.maxsize) },
    { "logwant",         INFO_FIELD_U32, offsetof(resolve_service_t, logger.want) },
    { "logtimestamp",    INFO_FIELD_U32, offsetof(resolve_service_t, logger.timestamp) },
    { "logrun",          INFO_FIELD_STR, offsetof(resolve_service_t, logger.execute.run.run) },
    { "logrun_user",     INFO_FIELD_STR, offsetof(resolve_service_t, logger.execute.run.run_user) },
    { "logrun_build",    INFO_FIELD_STR, offsetof(resolve_service_t, logger.execute.run.build) },
    { "logrun_runas",    INFO_FIELD_STR, offsetof(resolve_service_t, logger.execute.run.runas) },
    { "logtimeoutstart", INFO_FIELD_U32, offsetof(resolve_service_t, logger.execute.timeout.start) },
    { "logtimeoutstop",  INFO_FIELD_U32, offsetof(resolve_service_t, logger.execute.timeout.stop) },

    { "env",             INFO_FIELD_STR, offsetof(resolve_service_t, environ.env) },
    { "envdir",          INFO_FIELD_STR, offsetof(resolve_service_t, environ.envdir) },
    { "env_overwrite",   INFO_FIELD_U32, offsetof(resolve_service_t, environ.env_overwrite) },
    { "importfile",      INFO_FIELD_STR, offsetof(resolve_service_t, environ.importfile) },
    { "nimportfile",     INFO_FIELD_U32, offsetof(resolve_service_t, environ.nimportfile) },

    { "configure",       INFO_FIELD_STR, offsetof(resolve_service_t, regex.configure) },
    { "directories",     INFO_FIELD_STR, offsetof(resolve_service_t, regex.directories) },
    { "files",           INFO_FIELD_STR, offsetof(resolve_service_t, regex.files) },
    { "infiles",         INFO_FIELD_STR, offsetof(resolve_service_t, regex.infiles) },
    { "ndirectories",    INFO_FIELD_U32, offsetof(resolve_service_t, regex.ndirectories) },
    { "nfiles",          INFO_FIELD_U32, offsetof(resolve_service_t, regex.nfiles) },
    { "ninfiles",        INFO_FIELD_U32, offsetof(resolve_service_t, regex.ninfiles) },

    { "stdintype",       INFO_FIELD_U32, offsetof(resolve_service_t, io.fdin.type) },
    { "stdindest",       INFO_FIELD_STR, offsetof(resolve_service_t, io.fdin.destination) },
    { "stdouttype",      INFO_FIELD_U32, offsetof(resolve_service_t, io.fdout.type) },
    { "stdoutdest",      INFO_FIELD_STR, offsetof(resolve_service_t, io.fdout.destination) },
    { "stderrtype",      INFO_FIELD_U32, offsetof(resolve_service_t, io.fderr.type) },
    { "stderrdest",      INFO_FIELD_STR, offsetof(resolve_service_t, io.fderr.destination) },

    { "limitas",         INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitas) },
    { "limitcore",       INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitcore) },
    { "limitcpu",        INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitcpu) },
    { "limitdata",       INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitdata) },
    { "limitfsize",      INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitfsize) },
    { "limitlocks",      INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitlocks) },
    { "limitmemlock",    INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitmemlock) },
    { "limitmsgqueue",   INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitmsgqueue) },
    { "limitnice",       INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitnice) },
    { "limitnofile",     INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitnofile) },
    { "limitnproc",      INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitnproc) },
    { "limitrtprio",     INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitrtprio) },
    { "limitrttime",     INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitrttime) },
    { "limitsigpending", INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitsigpending) },
    { "limitstack",      INFO_FIELD_U64, offsetof(resolve_service_t, limit.limitstack) },

    { "rversion",        INFO_FIELD_STR, offsetof(resolve_service_t, rversion) },
} ;

/* option state, set by on_resolve, drained at the top of ssexec_resolve */
static char const *opt_field = 0 ;
static uint8_t opt_noname = 0 ;

static opt_t const opts_resolve[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "field",   .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name", .arg = OPT_NONE,                             .help = "display only the value, not the field name" },
} ;

static int on_resolve(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'f' : opt_field = arg ; break ;
        case 'n' : opt_noname = 1 ; break ;
        default: break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_resolve = {
    .name = "66 resolve",
    .help = "display the resolve file contents of services",
    .operands = "service",
    .opts = opts_resolve,
    .nopts = OPT_COUNT(opts_resolve),
    .on_option = &on_resolve,
    .fn = &ssexec_resolve,
    .epilog =
        "-f selects fields by name: the valid names are the field names a plain\n"
        "'66 resolve <service>' prints.",
} ;

int ssexec_resolve(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy */
    char const *field = opt_field ;
    uint8_t noname = opt_noname ;
    opt_field = 0 ;
    opt_noname = 0 ;

    int r = 0 ;
    char const *svname = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    svname = *argv ;

    if (svname[0] == '/') {

        char basename[strlen(svname) + 1] ;
        char dirname[strlen(svname) + 1] ;

        if (!ob_basename(basename, svname))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", svname) ;

        if (!ob_dirname(dirname, svname))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", svname) ;

        if (resolve_read_cdb(wres, dirname, basename) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file") ;

    } else {

        r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
        else if (!r || r == STATE_FLAGS_FALSE)
            log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it first using '66 parse ", svname, "'") ;

        if (resolve_read_g(wres, info->base.s, svname) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file") ;
    }

    info_resolve_display(&res, res.sa.s, fields, OPT_COUNT(fields), field, noname) ;

    resolve_free(wres) ;

    return 0 ;
}
