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
#include <stdlib.h> // free

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

static info_field_t const fields[] = {
    { "name",            INFO_FIELD_STR, offsetof(resolve_service_t, name), 0 },
    { "description",     INFO_FIELD_STR, offsetof(resolve_service_t, description), 0 },
    { "version",         INFO_FIELD_STR, offsetof(resolve_service_t, version), 0 },
    { "type",            INFO_FIELD_U32, offsetof(resolve_service_t, type), 0 },
    { "earlier",         INFO_FIELD_U32, offsetof(resolve_service_t, earlier), 0 },
    { "copyfrom",        INFO_FIELD_STR, offsetof(resolve_service_t, copyfrom), 0 },
    { "intree",          INFO_FIELD_STR, offsetof(resolve_service_t, intree), 0 },
    { "ownerstr",        INFO_FIELD_STR, offsetof(resolve_service_t, ownerstr), 0 },
    { "owner",           INFO_FIELD_U32, offsetof(resolve_service_t, owner), 0 },
    { "treename",        INFO_FIELD_STR, offsetof(resolve_service_t, treename), 0 },
    { "user",            INFO_FIELD_STR, offsetof(resolve_service_t, user), 0 },
    { "inns",            INFO_FIELD_STR, offsetof(resolve_service_t, inns), 0 },
    { "enabled",         INFO_FIELD_U32, offsetof(resolve_service_t, enabled), 0 },
    { "islog",           INFO_FIELD_U32, offsetof(resolve_service_t, islog), 0 },
    { "logger",          INFO_FIELD_U32, offsetof(resolve_service_t, logger), 0 },
    { "has_limit",       INFO_FIELD_U32, offsetof(resolve_service_t, has_limit), 0 },
    { "has_environ",     INFO_FIELD_U32, offsetof(resolve_service_t, has_environ), 0 },
    { "has_io",          INFO_FIELD_U32, offsetof(resolve_service_t, has_io), 0 },
    { "has_execute",     INFO_FIELD_U32, offsetof(resolve_service_t, has_execute), 0 },
    { "has_dependencies",INFO_FIELD_U32, offsetof(resolve_service_t, has_dependencies), 0 },
    { "has_regex",       INFO_FIELD_U32, offsetof(resolve_service_t, has_regex), 0 },
    { "has_event",       INFO_FIELD_U32, offsetof(resolve_service_t, has_event), 0 },

    { "home",            INFO_FIELD_STR, offsetof(resolve_service_t, path.home), 0 },
    { "frontend",        INFO_FIELD_STR, offsetof(resolve_service_t, path.frontend), 0 },
    { "src_servicedir",  INFO_FIELD_STR, offsetof(resolve_service_t, path.servicedir), 0 },

    { "depends",         INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, depends),     DATA_SERVICE_DEPENDENCIES },
    { "requiredby",      INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, requiredby),  DATA_SERVICE_DEPENDENCIES },
    { "optsdeps",        INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, optsdeps),    DATA_SERVICE_DEPENDENCIES },
    { "contents",        INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, contents),    DATA_SERVICE_DEPENDENCIES },
    { "provide",         INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, provide),     DATA_SERVICE_DEPENDENCIES },
    { "conflict",        INFO_FIELD_STR, offsetof(resolve_service_addon_dependencies_t, conflict),    DATA_SERVICE_DEPENDENCIES },
    { "ndepends",        INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, ndepends),    DATA_SERVICE_DEPENDENCIES },
    { "nrequiredby",     INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, nrequiredby), DATA_SERVICE_DEPENDENCIES },
    { "noptsdeps",       INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, noptsdeps),   DATA_SERVICE_DEPENDENCIES },
    { "ncontents",       INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, ncontents),   DATA_SERVICE_DEPENDENCIES },
    { "nprovide",        INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, nprovide),    DATA_SERVICE_DEPENDENCIES },
    { "nconflict",       INFO_FIELD_U32, offsetof(resolve_service_addon_dependencies_t, nconflict),   DATA_SERVICE_DEPENDENCIES },

    { "run",             INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, run.run),         DATA_SERVICE_EXECUTE },
    { "run_user",        INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, run.run_user),    DATA_SERVICE_EXECUTE },
    { "run_build",       INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, run.build),       DATA_SERVICE_EXECUTE },
    { "run_runas",       INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, run.runas),       DATA_SERVICE_EXECUTE },
    { "finish",          INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, finish.run),      DATA_SERVICE_EXECUTE },
    { "finish_user",     INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, finish.run_user), DATA_SERVICE_EXECUTE },
    { "finish_build",    INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, finish.build),    DATA_SERVICE_EXECUTE },
    { "finish_runas",    INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, finish.runas),    DATA_SERVICE_EXECUTE },
    { "timeoutstart",    INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, timeout.start),   DATA_SERVICE_EXECUTE },
    { "timeoutstop",     INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, timeout.stop),    DATA_SERVICE_EXECUTE },
    { "down",            INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, down),            DATA_SERVICE_EXECUTE },
    { "downsignal",      INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, downsignal),      DATA_SERVICE_EXECUTE },
    { "blockprivileges", INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, blockprivileges),DATA_SERVICE_EXECUTE },
    { "umask",           INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, umask),           DATA_SERVICE_EXECUTE },
    { "want_umask",      INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, want_umask),      DATA_SERVICE_EXECUTE },
    { "nice",            INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, nice),            DATA_SERVICE_EXECUTE },
    { "want_nice",       INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, want_nice),       DATA_SERVICE_EXECUTE },
    { "chdir",           INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, chdir),           DATA_SERVICE_EXECUTE },
    { "capsbound",       INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, capsbound),       DATA_SERVICE_EXECUTE },
    { "capsambient",     INFO_FIELD_STR, offsetof(resolve_service_addon_execute_t, capsambient),     DATA_SERVICE_EXECUTE },
    { "ncapsbound",      INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, ncapsbound),      DATA_SERVICE_EXECUTE },
    { "ncapsambient",    INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, ncapsambient),    DATA_SERVICE_EXECUTE },
    { "notify",          INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, notify),          DATA_SERVICE_EXECUTE },
    { "maxdeath",        INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, maxdeath),        DATA_SERVICE_EXECUTE },
    { "maxdeathtime",    INFO_FIELD_U32, offsetof(resolve_service_addon_execute_t, maxdeathtime),    DATA_SERVICE_EXECUTE },

    { "livedir",         INFO_FIELD_STR, offsetof(resolve_service_t, live.livedir), 0 },
    { "status",          INFO_FIELD_STR, offsetof(resolve_service_t, live.status), 0 },
    { "live_servicedir", INFO_FIELD_STR, offsetof(resolve_service_t, live.servicedir), 0 },
    { "scandir",         INFO_FIELD_STR, offsetof(resolve_service_t, live.scandir), 0 },
    { "statedir",        INFO_FIELD_STR, offsetof(resolve_service_t, live.statedir), 0 },
    { "eventdir",        INFO_FIELD_STR, offsetof(resolve_service_t, live.eventdir), 0 },
    { "supervisedir",    INFO_FIELD_STR, offsetof(resolve_service_t, live.supervisedir), 0 },
    { "fdholderdir",     INFO_FIELD_STR, offsetof(resolve_service_t, live.fdholderdir), 0 },
    { "oneshotddir",     INFO_FIELD_STR, offsetof(resolve_service_t, live.oneshotddir), 0 },
    { "eventddir",       INFO_FIELD_STR, offsetof(resolve_service_t, live.eventddir), 0 },

    { "env",             INFO_FIELD_STR, offsetof(resolve_service_addon_environ_t, env),           DATA_SERVICE_ENVIRON },
    { "envdir",          INFO_FIELD_STR, offsetof(resolve_service_addon_environ_t, envdir),        DATA_SERVICE_ENVIRON },
    { "env_overwrite",   INFO_FIELD_U32, offsetof(resolve_service_addon_environ_t, env_overwrite), DATA_SERVICE_ENVIRON },
    { "importfile",      INFO_FIELD_STR, offsetof(resolve_service_addon_environ_t, importfile),    DATA_SERVICE_ENVIRON },
    { "nimportfile",     INFO_FIELD_U32, offsetof(resolve_service_addon_environ_t, nimportfile),   DATA_SERVICE_ENVIRON },

    { "configure",       INFO_FIELD_STR, offsetof(resolve_service_addon_regex_t, configure),    DATA_SERVICE_REGEX },
    { "directories",     INFO_FIELD_STR, offsetof(resolve_service_addon_regex_t, directories),  DATA_SERVICE_REGEX },
    { "files",           INFO_FIELD_STR, offsetof(resolve_service_addon_regex_t, files),        DATA_SERVICE_REGEX },
    { "infiles",         INFO_FIELD_STR, offsetof(resolve_service_addon_regex_t, infiles),      DATA_SERVICE_REGEX },
    { "ndirectories",    INFO_FIELD_U32, offsetof(resolve_service_addon_regex_t, ndirectories), DATA_SERVICE_REGEX },
    { "nfiles",          INFO_FIELD_U32, offsetof(resolve_service_addon_regex_t, nfiles),       DATA_SERVICE_REGEX },
    { "ninfiles",        INFO_FIELD_U32, offsetof(resolve_service_addon_regex_t, ninfiles),     DATA_SERVICE_REGEX },

    { "stdintype",       INFO_FIELD_U32, offsetof(resolve_service_addon_io_t, fdin.type),         DATA_SERVICE_IO },
    { "stdindest",       INFO_FIELD_STR, offsetof(resolve_service_addon_io_t, fdin.destination),  DATA_SERVICE_IO },
    { "stdouttype",      INFO_FIELD_U32, offsetof(resolve_service_addon_io_t, fdout.type),        DATA_SERVICE_IO },
    { "stdoutdest",      INFO_FIELD_STR, offsetof(resolve_service_addon_io_t, fdout.destination), DATA_SERVICE_IO },
    { "stderrtype",      INFO_FIELD_U32, offsetof(resolve_service_addon_io_t, fderr.type),        DATA_SERVICE_IO },
    { "stderrdest",      INFO_FIELD_STR, offsetof(resolve_service_addon_io_t, fderr.destination), DATA_SERVICE_IO },

    { "limitas",         INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitas),        DATA_SERVICE_LIMIT },
    { "limitcore",       INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitcore),      DATA_SERVICE_LIMIT },
    { "limitcpu",        INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitcpu),       DATA_SERVICE_LIMIT },
    { "limitdata",       INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitdata),      DATA_SERVICE_LIMIT },
    { "limitfsize",      INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitfsize),     DATA_SERVICE_LIMIT },
    { "limitlocks",      INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitlocks),     DATA_SERVICE_LIMIT },
    { "limitmemlock",    INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitmemlock),   DATA_SERVICE_LIMIT },
    { "limitmsgqueue",   INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitmsgqueue),  DATA_SERVICE_LIMIT },
    { "limitnice",       INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitnice),      DATA_SERVICE_LIMIT },
    { "limitnofile",     INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitnofile),    DATA_SERVICE_LIMIT },
    { "limitnproc",      INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitnproc),     DATA_SERVICE_LIMIT },
    { "limitrtprio",     INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitrtprio),    DATA_SERVICE_LIMIT },
    { "limitrttime",     INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitrttime),    DATA_SERVICE_LIMIT },
    { "limitsigpending", INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitsigpending),DATA_SERVICE_LIMIT },
    { "limitstack",      INFO_FIELD_U64, offsetof(resolve_service_addon_limit_t, limitstack),     DATA_SERVICE_LIMIT },

    { "eventtype",       INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, type),       DATA_SERVICE_EVENT },
    { "eventfrom",       INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, from),       DATA_SERVICE_EVENT },
    { "neventfrom",      INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, nfrom),      DATA_SERVICE_EVENT },
    { "eventon",         INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, on),         DATA_SERVICE_EVENT },
    { "neventon",        INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, non),        DATA_SERVICE_EVENT },
    { "eventcombine",    INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, combine),    DATA_SERVICE_EVENT },
    { "eventdo",         INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, docmd),      DATA_SERVICE_EVENT },
    { "eventemit",       INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, emit),       DATA_SERVICE_EVENT },
    { "eventwatch",      INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, watch),      DATA_SERVICE_EVENT },
    { "eventexpression", INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, expression), DATA_SERVICE_EVENT },
    { "eventtimezone",   INFO_FIELD_STR, offsetof(resolve_service_addon_event_t, timezone),   DATA_SERVICE_EVENT },
    { "eventinterval",   INFO_FIELD_U32, offsetof(resolve_service_addon_event_t, interval),   DATA_SERVICE_EVENT },

    { "rversion",        INFO_FIELD_STR, offsetof(resolve_service_t, rversion), 0 },
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

        if (resolve_read(wres, info->base.s, svname) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file") ;
    }

    /* addons a field may live in, indexed by addon id; loaded on demand */
    resolve_service_addon_limit_t limit = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_service_addon_environ_t environ = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_service_addon_execute_t execute = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_service_addon_dependencies_t dependencies = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_service_addon_regex_t regex = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    resolve_service_addon_event_t event = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    info_addon_t addons[DATA_SERVICE_EVENT + 1] = {{0,0}} ;

    resolve_wrapper_t_ref wlimit = resolve_set_struct(DATA_SERVICE_LIMIT, &limit) ;
    if (res.has_limit && resolve_read(wlimit, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_LIMIT].base = &limit ;
        addons[DATA_SERVICE_LIMIT].blob = limit.sa.s ;
    }

    resolve_wrapper_t_ref wenviron = resolve_set_struct(DATA_SERVICE_ENVIRON, &environ) ;
    if (res.has_environ && resolve_read(wenviron, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_ENVIRON].base = &environ ;
        addons[DATA_SERVICE_ENVIRON].blob = environ.sa.s ;
    }

    resolve_wrapper_t_ref wio = resolve_set_struct(DATA_SERVICE_IO, &io) ;
    if (res.has_io && resolve_read(wio, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_IO].base = &io ;
        addons[DATA_SERVICE_IO].blob = io.sa.s ;
    }

    resolve_wrapper_t_ref wexecute = resolve_set_struct(DATA_SERVICE_EXECUTE, &execute) ;
    if (res.has_execute && resolve_read(wexecute, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_EXECUTE].base = &execute ;
        addons[DATA_SERVICE_EXECUTE].blob = execute.sa.s ;
    }

    resolve_wrapper_t_ref wdependencies = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dependencies) ;
    if (res.has_dependencies && resolve_read(wdependencies, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_DEPENDENCIES].base = &dependencies ;
        addons[DATA_SERVICE_DEPENDENCIES].blob = dependencies.sa.s ;
    }

    resolve_wrapper_t_ref wregex = resolve_set_struct(DATA_SERVICE_REGEX, &regex) ;
    if (res.has_regex && resolve_read(wregex, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_REGEX].base = &regex ;
        addons[DATA_SERVICE_REGEX].blob = regex.sa.s ;
    }

    resolve_wrapper_t_ref wevent = resolve_set_struct(DATA_SERVICE_EVENT, &event) ;
    if (res.has_event && resolve_read(wevent, res.sa.s + res.path.home, res.sa.s + res.name) > 0) {
        addons[DATA_SERVICE_EVENT].base = &event ;
        addons[DATA_SERVICE_EVENT].blob = event.sa.s ;
    }

    info_resolve_display(&res, res.sa.s, fields, OPT_COUNT(fields), field, noname, addons, DATA_SERVICE_EVENT + 1) ;

    resolve_free(wlimit) ;
    resolve_free(wenviron) ;
    resolve_free(wio) ;
    resolve_free(wexecute) ;
    resolve_free(wdependencies) ;
    resolve_free(wregex) ;
    resolve_free(wevent) ;
    resolve_free(wres) ;

    return 0 ;
}
