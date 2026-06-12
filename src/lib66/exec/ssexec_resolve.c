/*
 * ssexec_resolve.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <wchar.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/stream.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/info.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

/* One row per cdb key, in the exact order and with the exact names written by
 * service_resolve_write_cdb.c -- the key is what lives in the resolve file and
 * is what -f selects (and what a future modify-by-field would target). The type
 * tells how to render the value held in the resolve_service_t: a T_STR field is
 * a uint32 offset into res->sa, a T_U32 field is the value itself, a T_U64 field
 * (the limits) is a 64-bit value. */

enum field_type_e { T_STR, T_U32, T_U64 } ;

typedef struct field_s field_t ;
struct field_s {
    char const *key ;
    uint8_t type ;
    size_t offset ;
} ;

static field_t const fields[] = {
    { "name",            T_STR, offsetof(resolve_service_t, name) },
    { "description",     T_STR, offsetof(resolve_service_t, description) },
    { "version",         T_STR, offsetof(resolve_service_t, version) },
    { "type",            T_U32, offsetof(resolve_service_t, type) },
    { "notify",          T_U32, offsetof(resolve_service_t, notify) },
    { "maxdeath",        T_U32, offsetof(resolve_service_t, maxdeath) },
    { "earlier",         T_U32, offsetof(resolve_service_t, earlier) },
    { "copyfrom",        T_STR, offsetof(resolve_service_t, copyfrom) },
    { "intree",          T_STR, offsetof(resolve_service_t, intree) },
    { "ownerstr",        T_STR, offsetof(resolve_service_t, ownerstr) },
    { "owner",           T_U32, offsetof(resolve_service_t, owner) },
    { "treename",        T_STR, offsetof(resolve_service_t, treename) },
    { "user",            T_STR, offsetof(resolve_service_t, user) },
    { "inns",            T_STR, offsetof(resolve_service_t, inns) },
    { "enabled",         T_U32, offsetof(resolve_service_t, enabled) },
    { "islog",           T_U32, offsetof(resolve_service_t, islog) },

    { "home",            T_STR, offsetof(resolve_service_t, path.home) },
    { "frontend",        T_STR, offsetof(resolve_service_t, path.frontend) },
    { "src_servicedir",  T_STR, offsetof(resolve_service_t, path.servicedir) },

    { "depends",         T_STR, offsetof(resolve_service_t, dependencies.depends) },
    { "requiredby",      T_STR, offsetof(resolve_service_t, dependencies.requiredby) },
    { "optsdeps",        T_STR, offsetof(resolve_service_t, dependencies.optsdeps) },
    { "contents",        T_STR, offsetof(resolve_service_t, dependencies.contents) },
    { "provide",         T_STR, offsetof(resolve_service_t, dependencies.provide) },
    { "conflict",        T_STR, offsetof(resolve_service_t, dependencies.conflict) },
    { "ndepends",        T_U32, offsetof(resolve_service_t, dependencies.ndepends) },
    { "nrequiredby",     T_U32, offsetof(resolve_service_t, dependencies.nrequiredby) },
    { "noptsdeps",       T_U32, offsetof(resolve_service_t, dependencies.noptsdeps) },
    { "ncontents",       T_U32, offsetof(resolve_service_t, dependencies.ncontents) },
    { "nprovide",        T_U32, offsetof(resolve_service_t, dependencies.nprovide) },
    { "nconflict",       T_U32, offsetof(resolve_service_t, dependencies.nconflict) },

    { "run",             T_STR, offsetof(resolve_service_t, execute.run.run) },
    { "run_user",        T_STR, offsetof(resolve_service_t, execute.run.run_user) },
    { "run_build",       T_STR, offsetof(resolve_service_t, execute.run.build) },
    { "run_runas",       T_STR, offsetof(resolve_service_t, execute.run.runas) },
    { "finish",          T_STR, offsetof(resolve_service_t, execute.finish.run) },
    { "finish_user",     T_STR, offsetof(resolve_service_t, execute.finish.run_user) },
    { "finish_build",    T_STR, offsetof(resolve_service_t, execute.finish.build) },
    { "finish_runas",    T_STR, offsetof(resolve_service_t, execute.finish.runas) },
    { "timeoutstart",    T_U32, offsetof(resolve_service_t, execute.timeout.start) },
    { "timeoutstop",     T_U32, offsetof(resolve_service_t, execute.timeout.stop) },
    { "down",            T_U32, offsetof(resolve_service_t, execute.down) },
    { "downsignal",      T_U32, offsetof(resolve_service_t, execute.downsignal) },
    { "blockprivileges", T_U32, offsetof(resolve_service_t, execute.blockprivileges) },
    { "umask",           T_U32, offsetof(resolve_service_t, execute.umask) },
    { "want_umask",      T_U32, offsetof(resolve_service_t, execute.want_umask) },
    { "nice",            T_U32, offsetof(resolve_service_t, execute.nice) },
    { "want_nice",       T_U32, offsetof(resolve_service_t, execute.want_nice) },
    { "chdir",           T_STR, offsetof(resolve_service_t, execute.chdir) },
    { "capsbound",       T_STR, offsetof(resolve_service_t, execute.capsbound) },
    { "capsambient",     T_STR, offsetof(resolve_service_t, execute.capsambient) },
    { "ncapsbound",      T_U32, offsetof(resolve_service_t, execute.ncapsbound) },
    { "ncapsambient",    T_U32, offsetof(resolve_service_t, execute.ncapsambient) },

    { "livedir",         T_STR, offsetof(resolve_service_t, live.livedir) },
    { "status",          T_STR, offsetof(resolve_service_t, live.status) },
    { "live_servicedir", T_STR, offsetof(resolve_service_t, live.servicedir) },
    { "scandir",         T_STR, offsetof(resolve_service_t, live.scandir) },
    { "statedir",        T_STR, offsetof(resolve_service_t, live.statedir) },
    { "eventdir",        T_STR, offsetof(resolve_service_t, live.eventdir) },
    { "notifdir",        T_STR, offsetof(resolve_service_t, live.notifdir) },
    { "supervisedir",    T_STR, offsetof(resolve_service_t, live.supervisedir) },
    { "fdholderdir",     T_STR, offsetof(resolve_service_t, live.fdholderdir) },
    { "oneshotddir",     T_STR, offsetof(resolve_service_t, live.oneshotddir) },

    { "logname",         T_STR, offsetof(resolve_service_t, logger.name) },
    { "logbackup",       T_U32, offsetof(resolve_service_t, logger.backup) },
    { "logmaxsize",      T_U32, offsetof(resolve_service_t, logger.maxsize) },
    { "logwant",         T_U32, offsetof(resolve_service_t, logger.want) },
    { "logtimestamp",    T_U32, offsetof(resolve_service_t, logger.timestamp) },
    { "logrun",          T_STR, offsetof(resolve_service_t, logger.execute.run.run) },
    { "logrun_user",     T_STR, offsetof(resolve_service_t, logger.execute.run.run_user) },
    { "logrun_build",    T_STR, offsetof(resolve_service_t, logger.execute.run.build) },
    { "logrun_runas",    T_STR, offsetof(resolve_service_t, logger.execute.run.runas) },
    { "logtimeoutstart", T_U32, offsetof(resolve_service_t, logger.execute.timeout.start) },
    { "logtimeoutstop",  T_U32, offsetof(resolve_service_t, logger.execute.timeout.stop) },

    { "env",             T_STR, offsetof(resolve_service_t, environ.env) },
    { "envdir",          T_STR, offsetof(resolve_service_t, environ.envdir) },
    { "env_overwrite",   T_U32, offsetof(resolve_service_t, environ.env_overwrite) },
    { "importfile",      T_STR, offsetof(resolve_service_t, environ.importfile) },
    { "nimportfile",     T_U32, offsetof(resolve_service_t, environ.nimportfile) },

    { "configure",       T_STR, offsetof(resolve_service_t, regex.configure) },
    { "directories",     T_STR, offsetof(resolve_service_t, regex.directories) },
    { "files",           T_STR, offsetof(resolve_service_t, regex.files) },
    { "infiles",         T_STR, offsetof(resolve_service_t, regex.infiles) },
    { "ndirectories",    T_U32, offsetof(resolve_service_t, regex.ndirectories) },
    { "nfiles",          T_U32, offsetof(resolve_service_t, regex.nfiles) },
    { "ninfiles",        T_U32, offsetof(resolve_service_t, regex.ninfiles) },

    { "stdintype",       T_U32, offsetof(resolve_service_t, io.fdin.type) },
    { "stdindest",       T_STR, offsetof(resolve_service_t, io.fdin.destination) },
    { "stdouttype",      T_U32, offsetof(resolve_service_t, io.fdout.type) },
    { "stdoutdest",      T_STR, offsetof(resolve_service_t, io.fdout.destination) },
    { "stderrtype",      T_U32, offsetof(resolve_service_t, io.fderr.type) },
    { "stderrdest",      T_STR, offsetof(resolve_service_t, io.fderr.destination) },

    { "limitas",         T_U64, offsetof(resolve_service_t, limit.limitas) },
    { "limitcore",       T_U64, offsetof(resolve_service_t, limit.limitcore) },
    { "limitcpu",        T_U64, offsetof(resolve_service_t, limit.limitcpu) },
    { "limitdata",       T_U64, offsetof(resolve_service_t, limit.limitdata) },
    { "limitfsize",      T_U64, offsetof(resolve_service_t, limit.limitfsize) },
    { "limitlocks",      T_U64, offsetof(resolve_service_t, limit.limitlocks) },
    { "limitmemlock",    T_U64, offsetof(resolve_service_t, limit.limitmemlock) },
    { "limitmsgqueue",   T_U64, offsetof(resolve_service_t, limit.limitmsgqueue) },
    { "limitnice",       T_U64, offsetof(resolve_service_t, limit.limitnice) },
    { "limitnofile",     T_U64, offsetof(resolve_service_t, limit.limitnofile) },
    { "limitnproc",      T_U64, offsetof(resolve_service_t, limit.limitnproc) },
    { "limitrtprio",     T_U64, offsetof(resolve_service_t, limit.limitrtprio) },
    { "limitrttime",     T_U64, offsetof(resolve_service_t, limit.limitrttime) },
    { "limitsigpending", T_U64, offsetof(resolve_service_t, limit.limitsigpending) },
    { "limitstack",      T_U64, offsetof(resolve_service_t, limit.limitstack) },

    { "rversion",        T_STR, offsetof(resolve_service_t, rversion) },
} ;

static wchar_t const field_suffix[] = L" :" ;

/* option state, set by on_resolve, drained at the top of ssexec_resolve */
static char const *opt_field = 0 ;
static uint8_t opt_noname = 0 ;

/* runtime flag honoured by the display helpers (drained from opt_noname) */
static uint8_t noname = 0 ;

static void display_field_value(resolve_service_t const *res, field_t const *f)
{
    void const *p = (char const *)res + f->offset ;

    if (f->type == T_STR) {

        uint32_t off = *(uint32_t const *)p ;

        if (!off) {

            if (!ostream_fmt(ostream_1, "%s%s", log_color->warning, "None"))
                log_dieu(LOG_EXIT_SYS, "write to stdout") ;

        } else {

            if (!ostream_puts(ostream_1, res->sa.s + off))
                log_dieu(LOG_EXIT_SYS, "write to stdout") ;
        }

    } else if (f->type == T_U32) {

        char ui[U32_FMT] ;
        ui[u32_fmt(ui, *(uint32_t const *)p)] = 0 ;

        if (!ostream_puts(ostream_1, ui))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    } else {

        char ui[U64_FMT] ;
        ui[u64_fmt(ui, *(uint64_t const *)p)] = 0 ;

        if (!ostream_puts(ostream_1, ui))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void display_field(resolve_service_t const *res, field_t const *f, char const *aligned)
{
    if (!noname)
        info_display_field_name(aligned) ;

    display_field_value(res, f) ;
}

static opt_t const opts_resolve[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",   .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "field",  .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'n',         .shortname = 'n', .longname = "noname", .arg = OPT_NONE,                             .help = "display only the value, not the field name" },
} ;

static int on_resolve(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'f' :

            opt_field = arg ;
            break ;

        case 'n' :

            opt_noname = 1 ;
            break ;
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
} ;

int ssexec_resolve(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy */
    char const *field = opt_field ;
    noname = opt_noname ;
    opt_field = 0 ;
    opt_noname = 0 ;

    int r = 0 ;
    char const *svname = 0 ;
    size_t const nfields = OPT_COUNT(fields) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    svname = *argv ;

    if (svname[0] == '/') {

        _alloc_strbuf_(basename, strlen(svname) + 1) ;
        _alloc_strbuf_(dirname, strlen(svname) + 1) ;

        if (!ob_basename(basename.s, svname))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", svname) ;

        if (!ob_dirname(dirname.s, svname))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", svname) ;

        if (resolve_read_cdb(wres, dirname.s, basename.s) <= 0)
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

    /* build the list of fields to display, in display order */
    size_t idx[nfields] ;
    size_t n = 0 ;

    if (field) {

        _alloc_sbl_(sel, 256) ;

        if (!opt_list(&sel, field, ','))
            log_dieu(LOG_EXIT_SYS, "parse field list: ", field) ;

        size_t pos = 0 ;
        FOREACH_SBL(&sel, pos) {

            char const *key = sel.s + pos ;
            size_t i = 0 ;

            for (; i < nfields ; i++)
                if (!strcmp(fields[i].key, key))
                    break ;

            if (i == nfields)
                log_die(LOG_EXIT_USER, "unknown field: ", key) ;

            idx[n++] = i ;
        }

    } else {

        for (; n < nfields ; n++)
            idx[n] = n ;
    }

    if (noname) {

        for (size_t i = 0 ; i < n ; i++)
            display_field(&res, &fields[idx[i]], 0) ;

    } else {

        char buf[nfields][INFO_FIELD_MAXLEN] ;
        char aligned[nfields][INFO_FIELD_MAXLEN] ;

        for (size_t i = 0 ; i < n ; i++) {

            char const *key = fields[idx[i]].key ;
            memcpy(buf[i], key, strlen(key) + 1) ;
        }

        info_field_align(buf, aligned, field_suffix, n) ;

        for (size_t i = 0 ; i < n ; i++)
            display_field(&res, &fields[idx[i]], aligned[i]) ;
    }

    resolve_free(wres) ;

    return 0 ;
}
