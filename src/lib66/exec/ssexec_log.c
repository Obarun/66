/*
 * ssexec_log.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <regex.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/stream.h>
#include <oblibs/clock.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>

#include <66/ssexec.h>
#include <66/log.h>
#include <66/constants.h>
#include <66/enum_parser.h>
#include <66/service.h>
#include <66/resolve.h>

static char const *arg_since = 0 ;
static char const *arg_until = 0 ;
static char const *arg_grep = 0 ;

static opt_t const opts_log[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE,                         .help = "print this help" },
    { .id = 's',         .shortname = 's', .longname = "since", .arg = OPT_REQUIRED, .argname = "time",  .help = "only show lines at or after time" },
    { .id = 'u',         .shortname = 'u', .longname = "until", .arg = OPT_REQUIRED, .argname = "time",  .help = "only show lines at or before time" },
    { .id = 'g',         .shortname = 'g', .longname = "grep",  .arg = OPT_REQUIRED, .argname = "regex", .help = "only show lines matching the POSIX extended regex" },
} ;

static int on_log(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 's' : arg_since = arg ; break ;
        case 'u' : arg_until = arg ; break ;
        case 'g' : arg_grep = arg ; break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_log = {
    .name = "66 log",
    .help = "display service and system logs",
    .operands = "service|system",
    .opts = opts_log,
    .nopts = OPT_COUNT(opts_log),
    .on_option = &on_log,
    .fn = &ssexec_log,
    .epilog =
        "operand:\n"
        "    (none)     interleave every service plus the system catch-all\n"
        "    system     the boot/scandir catch-all logger\n"
        "    <service>  a single service, by name\n",
} ;

static int ts_cmp(struct timespec const *a, struct timespec const *b)
{
    if (a->tv_sec != b->tv_sec)
        return a->tv_sec < b->tv_sec ? -1 : 1 ;
    if (a->tv_nsec != b->tv_nsec)
        return a->tv_nsec < b->tv_nsec ? -1 : 1 ;
    return 0 ;
}

static int parse_time(char const *str, struct timespec *ts)
{
    size_t end, len = strlen(str) ;
    /* CLI form is unquoted ISO 8601: only 'T'/'t' joins date and time. The DFA
     * also accepts a space (for log lines), but a space here would force shell
     * quoting, so reject it -- one canonical command-line format. */
    if (strchr(str, ' '))
        return 0 ;

    return log_iso_scan(str, len, ts, &end) && end == len ;
}

static void emit_line(log_source_t *src, log_line_t *pline, uint8_t withname)
{
    char *base = src->data.s + pline->off ;
    int ok = 1 ;

    // emit the timestamp first, then the source name (syslog-style TAG), then the
    // message; for TAI64N the stamp is reformatted to local time, otherwise the
    // line's own stamp (bytes up to msgoff, empty for an unstamped line) is kept
    if (pline->type == LOG_STAMP_TAI64N) {

        char local[CLOCK_LOCAL_LEN + 1] ;
        size_t ll = clock_local_fmt(local, &pline->stamp) ;

        ok = ostream_put(ostream_1, local, ll)
          && ostream_put(ostream_1, " ", 1) ;

    } else {
        ok = ostream_put(ostream_1, base, pline->msgoff) ;
    }

    if (ok && withname)
        ok = ostream_puts(ostream_1, src->name)
          && ostream_put(ostream_1, ": ", 2) ;

    if (ok)
        ok = ostream_put(ostream_1, base + pline->msgoff, pline->len - pline->msgoff) ;

    if (!ok || !ostream_put(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static log_source_t *collect_all(ssexec_t *info, size_t *nsrc)
{
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    char solve[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1] ;
    char const *exclude[1] = { 0 } ;
    log_source_t *src = 0 ;
    size_t n = 0, pos = 0 ;

    auto_strings(solve, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    if (!sbl_dir_get_recursive(&names, solve, exclude, S_IFLNK, 0) && errno != ENOENT)
        log_dieusys(LOG_EXIT_SYS, "list resolve directory: ", solve) ;

    src = malloc((sbl_count(&names) + 1) * sizeof(log_source_t)) ;
    if (!src)
        log_die_nomem("source list") ;

    FOREACH_SBL(&names, pos) {

        char const *name = names.s + pos ;
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        if (resolve_read_g(wres, info->base.s, name) <= 0) {
            resolve_free(wres) ;
            continue ;
        }

        char const *dest = res.sa.s + res.io.fdout.destination ;

        if (!res.islog && res.io.fdout.type == E_PARSER_IO_TYPE_66LOG && scan_mode(dest, S_IFDIR) == 1) {

            src[n] = (log_source_t)LOG_SOURCE_ZERO ;
            if (!log_source_logdir(&src[n], name, dest))
                log_dieusys(LOG_EXIT_SYS, "read log directory of: ", name) ;

            n++ ;

        } else if (!res.islog && res.io.fdout.type == E_PARSER_IO_TYPE_FILE && scan_mode(dest, S_IFREG) == 1) {

            src[n] = (log_source_t)LOG_SOURCE_ZERO ;
            if (!log_source_file(&src[n], name, dest))
                log_dieusys(LOG_EXIT_SYS, "read log file of: ", name) ;

            n++ ;
        }

        resolve_free(wres) ;
    }

    char p[info->live.len + SS_LOG_LEN + 1 + info->ownerlen + 1] ;
    auto_strings(p, info->live.s, SS_LOG, "/", info->ownerstr) ;

    if (scan_mode(p, S_IFDIR) == 1) {

        src[n] = (log_source_t)LOG_SOURCE_ZERO ;
        if (!log_source_logdir(&src[n], SS_SYSTEM, p))
            log_dieusys(LOG_EXIT_SYS, "read system log directory: ", p) ;

        n++ ;
    }

    *nsrc = n ;

    return src ;
}

static log_source_t *collect_system(ssexec_t *info, size_t *nsrc)
{
    log_source_t *src = malloc(sizeof(log_source_t)) ;
    if (!src)
        log_die_nomem("malloc") ;

    *src = (log_source_t)LOG_SOURCE_ZERO ;

    char p[info->live.len + SS_LOG_LEN + 1 + info->ownerlen + 1] ;
    auto_strings(p, info->live.s, SS_LOG, "/", info->ownerstr) ;

    if (!log_source_logdir(&src[0], SS_SYSTEM, p))
        log_dieusys(LOG_EXIT_SYS, "read system log directory: ", p) ;

    *nsrc = 1 ;

    return src ;
}

static log_source_t *collect_service(ssexec_t *info, char const *name, size_t *nsrc)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    log_source_t *src = 0 ;
    int r ;

    r = resolve_read_g(wres, info->base.s, name) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", name) ;
    if (!r)
        log_die(LOG_EXIT_USER, "unknown service: ", name) ;

    src = malloc(sizeof(log_source_t)) ;
    if (!src)
        log_die_nomem("malloc") ;

    *src = (log_source_t)LOG_SOURCE_ZERO ;

    char const *dest = res.sa.s + res.io.fdout.destination ;

    if (res.io.fdout.type == E_PARSER_IO_TYPE_66LOG) {

        if (!log_source_logdir(&src[0], name, dest))
            log_dieusys(LOG_EXIT_SYS, "read log directory of: ", name) ;

    } else if (res.io.fdout.type == E_PARSER_IO_TYPE_FILE) {

        if (!log_source_file(&src[0], name, dest))
            log_dieusys(LOG_EXIT_SYS, "read log file of: ", name) ;

    } else log_die(LOG_EXIT_USER, "service has no readable log: ", name) ;

    resolve_free(wres) ;

    *nsrc = 1 ;

    return src ;
}

int ssexec_log(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    char const *since = arg_since, *until = arg_until, *grep = arg_grep ;
    arg_since = arg_until = arg_grep = 0 ;

    struct timespec tsince, tuntil ;
    uint8_t withname = 0 ;
    regex_t re ;

    if (since) {

        if (!parse_time(since, &tsince))
            log_die(LOG_EXIT_USER, "invalid time (expected YYYY-MM-DDTHH:MM:SS): ", since) ;
    }

    if (until) {

        if (!parse_time(until, &tuntil))
            log_die(LOG_EXIT_USER, "invalid time (expected YYYY-MM-DDTHH:MM:SS): ", until) ;
    }

    if (grep) {

        if (regcomp(&re, grep, REG_EXTENDED | REG_NEWLINE))
            log_die(LOG_EXIT_USER, "invalid regular expression: ", grep) ;
    }

    char const *target = argc >= 1 ? argv[0] : 0 ;

    log_source_t *src = 0 ;
    size_t nsrc ;

    if (!target) {

        src = collect_all(info, &nsrc) ;
        withname = 1 ;

    } else if (!strcmp(target, SS_SYSTEM)) {

        src = collect_system(info, &nsrc) ;
        withname = 1 ;

    } else {

        src = collect_service(info, target, &nsrc) ;
    }

    for (;;) {

        size_t best = nsrc ;
        for (size_t i = 0 ; i < nsrc ; i++) {

            if (src[i].cur >= src[i].nline)
                continue ;

            if (best == nsrc || ts_cmp(&src[i].line[src[i].cur].stamp, &src[best].line[src[best].cur].stamp) < 0)
                best = i ;
        }

        if (best == nsrc)
            break ;

        log_source_t *s = &src[best] ;
        log_line_t *pline = &s->line[s->cur++] ;

        if (since && ts_cmp(&pline->stamp, &tsince) < 0)
            continue ;

        if (until && ts_cmp(&pline->stamp, &tuntil) > 0)
            continue ;

        if (grep) {
            char *line = s->data.s + pline->off ;
            char saved = line[pline->len] ;
            line[pline->len] = 0 ;
            int nomatch = regexec(&re, line, 0, 0, 0) ;
            line[pline->len] = saved ;
            if (nomatch)
                continue ;
        }

        emit_line(s, pline, withname) ;
    }

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    for (size_t i = 0 ; i < nsrc ; i++)
        log_source_free(&src[i]) ;

    free(src) ;

    if (grep)
        regfree(&re) ;

    return 0 ;
}
