/*
 * ssexec_status.c
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

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/stream.h>

#include <66/info.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/enum_parser.h>
#include <66/resolve.h>
#include <66/svc.h>
#include <66/state.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/status.h>
#include <66/log.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;

#define STATUS_NLOG 5

static void info_display_string(char const *str) ;
static void info_display_name(char const *field, resolve_service_t *res) ;
static void info_display_description(char const *field, resolve_service_t *res) ;
static void info_display_type(char const *field, resolve_service_t *res) ;
static void info_display_source(char const *field, resolve_service_t *res) ;
static void info_display_tree(char const *field, resolve_service_t *res) ;
static void info_display_status(char const *field, resolve_service_t *res) ;
static void info_display_enabled(char const *field, resolve_service_t *res) ;
static void info_display_pid(char const *field, resolve_service_t *res) ;
static uint8_t status_dependencies_load(resolve_service_addon_dependencies_t *dep, resolve_service_t *res) ;
static void info_display_deps(char const *field, resolve_service_t *res) ;
static void info_display_requiredby(char const *field, resolve_service_t *res) ;
static void info_display_contents(char const *field, resolve_service_t *res) ;
static void info_display_log(char const *field, resolve_service_t *res) ;

static info_graph_style *S_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;

/* One row per displayable field, in display order. The single source of truth:
 * key is what -f selects, label is the printed field name, render does the work.
 * Only what describes the state of a service lives here: its configuration is
 * '66 resolve' business, its full log '66 log' business. */

typedef struct status_field_s status_field_t ;
struct status_field_s {
    char const *key ;
    char const *label ;
    void (*render)(char const *field, resolve_service_t *res) ;
} ;

static status_field_t const fields_sv[] = {
    { "name",        "Name",                   &info_display_name },
    { "status",      "Status",                 &info_display_status },
    { "description", "Description",            &info_display_description },
    { "type",        "Type",                   &info_display_type },
    { "source",      "Source",                 &info_display_source },
    { "tree",        "Tree",                   &info_display_tree },

    { "enabled",     "Enabled",                &info_display_enabled },
    { "pid",         "Pid",                    &info_display_pid },
    { "depends",     "Dependencies",           &info_display_deps },
    { "requiredby",  "Required by",            &info_display_requiredby },
    { "contents",    "Contents",               &info_display_contents },
    { "log",         "Log",                    &info_display_log },
} ;

#define NFIELD OPT_COUNT(fields_sv)

static void info_display_string(char const *str)
{
    if (!ostream_puts(ostream_1,str) ||
        !ostream_putflush(ostream_1, "\n", 1))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_empty(void)
{
    if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_name(char const *field, resolve_service_t *res)
{
    log_flow() ;

    (void)field ;

    if (!NOFIELD) {

        info_display_string(res->sa.s + res->name) ;
        return ;
    }

    if (!ostream_fmt(ostream_1, "%s ( %s )\n", res->sa.s + res->name,
                     enum_to_key(enum_list_parser_type, res->type)))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void info_display_source(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;
    info_display_string(res->sa.s + res->path.frontend) ;
}

static void info_display_tree(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;
    info_display_string(res->sa.s + res->treename) ;
}

static uint8_t status_record_load(service_status_t *st, resolve_service_t *res)
{
    char const *supervisedir = res->sa.s + res->live.supervisedir ;
    char file[strlen(supervisedir) + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, supervisedir, "/", SS_STATUS) ;

    if (access(file, F_OK) < 0)
        return 0 ;

    if (status_read(st, file) < 0)
        log_dieusys(LOG_EXIT_SYS, "read status of: ", res->sa.s + res->name) ;

    return 1 ;
}

static void info_get_status(resolve_service_t *res)
{
    int warn_color = 0 ;
    service_status_t st = STATUS_ZERO ;

    if (!status_record_load(&st, res)) {

        if (NOFIELD && res->enabled && !ostream_puts(ostream_1, "enabled, "))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        return ;
    }

    char const *disen = !NOFIELD ? "" : res->enabled ? "enabled, " : "disabled, " ;

    uint8_t estate = svc_status_effective(res, &st) ;

    char const *word = status_state_to_string(estate) ;
    switch (estate) {
        case STATUS_STATE_UP :
        case STATUS_STATE_STARTING :
        case STATUS_STATE_DONE :
        case STATUS_STATE_WAITING :    warn_color = 2 ; break ;
        default :                      warn_color = 1 ; break ;
    }

    struct timespec now ;
    clock_now(&now) ;
    uint64_t elapsed = now.tv_sec > st.stamp.tv_sec ? (uint64_t)(now.tv_sec - st.stamp.tv_sec) : 0 ;
    char secs[INFO_DURATION_LEN + 1] ;

    if (NOFIELD)
        info_fmt_duration(secs, elapsed) ;
    else
        secs[u64_fmt(secs, elapsed)] = 0 ;

    // what last happened to the process; nothing to report on a clean success
    char code[U64_FMT] ;
    code[u64_fmt(code, st.code)] = 0 ;
    char detail[U64_FMT + 16] = "" ;
    switch (st.result) {
        case STATUS_RESULT_SUCCESS :
            break ;
        case STATUS_RESULT_EXITED :
        case STATUS_RESULT_SIGNALED :
            auto_strings(detail, " (", status_result_to_string(st.result), " ", code, ")") ;
            break ;
        default :
            auto_strings(detail, " (", status_result_to_string(st.result), ")") ;
            break ;
    }

    char whoby[16] = "" ;
    auto_strings(whoby, " by ", status_who_to_string(st.who)) ;

    /* a reactor goes back to waiting after each firing: its state alone never says
     * whether its Execute already ran, only the date of its last run does. */
    char lastrun[INFO_DURATION_LEN + 18] = "" ;
    if (st.state == STATUS_STATE_WAITING) {

        if (!st.readystamp.tv_sec) {

            auto_strings(lastrun, " (never run)") ;

        } else {

            uint64_t ran = now.tv_sec > st.readystamp.tv_sec ? (uint64_t)(now.tv_sec - st.readystamp.tv_sec) : 0 ;
            char ago[INFO_DURATION_LEN + 1] ;

            if (NOFIELD)
                info_fmt_duration(ago, ran) ;
            else
                ago[u64_fmt(ago, ran)] = 0 ;

            auto_strings(lastrun, " (last run ", ago, " ago)") ;
        }
    }

    char const *color = warn_color > 1 ? log_color->valid : log_color->error ;

    if (st.pid > 0) {

        char pid[PID_FMT] ;
        pid[pid_format(pid, st.pid)] = 0 ;

        if (!ostream_fmt(ostream_1, "%s%s%s%s (pid %s)%s since %s%s%s\n",
                         disen, color, word, log_color->off, pid, detail, secs, whoby, lastrun))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    } else {

        if (!ostream_fmt(ostream_1, "%s%s%s%s%s since %s%s%s\n",
                         disen, color, word, log_color->off, detail, secs, whoby, lastrun))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_status(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;
    info_get_status(res) ;
}

static void info_display_pid(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;

    service_status_t st = STATUS_ZERO ;
    char ui[U32_FMT] ;

    status_record_load(&st, res) ;

    ui[u32_fmt(ui, st.pid)] = 0 ;
    info_display_string(ui) ;
}

static void info_display_enabled(char const *field,resolve_service_t *res)
{
    log_flow() ;

    uint32_t disen = res->enabled ;

    (void)field ;

    if (!ostream_fmt(ostream_1, "%s%s%s\n", disen ? log_color->valid : log_color->warning, disen ? "yes" : "no", log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_type(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;
    info_display_string(enum_to_key(enum_list_parser_type, res->type)) ;
}

static void info_display_description(char const *field,resolve_service_t *res)
{
    log_flow() ;

    (void)field ;
    info_display_string(res->sa.s + res->description) ;
}

static void info_display_requiredby(char const *field, resolve_service_t *res)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t padding = 1 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY|GRAPH_COLLECT_PARSE, nservice = 0 ;

    padding = field ? info_length_from_wchar(field) + 1 : 0 ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    if (!status_dependencies_load(&dep, res) || !dep.nrequiredby) {
        strbuf_free(&dep.sa) ;
        goto empty ;
    }

    if (!sbl_clean_string(&sa, dep.sa.s + dep.requiredby)) {
        strbuf_free(&dep.sa) ;
        log_dieu(LOG_EXIT_SYS, "clean string") ;
    }
    strbuf_free(&dep.sa) ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_dieusys(LOG_EXIT_SYS, "collect resolve file of service: ", res->sa.s + res->name) ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk(&graph, 0, 0, 1, REVERSE, &d, padding, S_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the requiredby list") ;

        goto freed ;

    } else {

        uint32_t pos = 0 ;
        sa.len = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {
            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            if (!sbl_add(&sa, name))
                log_die_nomem("strbuf") ;
        }

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:
        if (GRAPH) {
            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        } else {
            info_display_empty() ;
        }
    freed:
        service_graph_destroy(&graph) ;
}

static void info_display_deps(char const *field, resolve_service_t *res)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t padding = 1 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_COLLECT_PARSE, nservice = 0 ;

    padding = field ? info_length_from_wchar(field) + 1 : 0 ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    if (!status_dependencies_load(&dep, res) || !dep.ndepends) {
        strbuf_free(&dep.sa) ;
        goto empty ;
    }

    if (!sbl_clean_string(&sa, dep.sa.s + dep.depends)) {
        strbuf_free(&dep.sa) ;
        log_dieu(LOG_EXIT_SYS, "clean string") ;
    }
    strbuf_free(&dep.sa) ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_dieusys(LOG_EXIT_SYS, "collect resolve file of service: ", res->sa.s + res->name) ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk(&graph, 0, 0, 0, REVERSE, &d, padding, S_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

        goto freed ;

    } else {

        uint32_t pos = 0 ;
        sa.len = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {
            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            if (!sbl_add(&sa, name))
                log_die_nomem("strbuf") ;
        }

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:
        if (GRAPH) {
            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        } else {
            info_display_empty() ;
        }

    freed:
        service_graph_destroy(&graph) ;

}

static void info_display_contents(char const *field, resolve_service_t *res)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t padding = 1 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t nservice = 0, flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY ;

    padding = field ? info_length_from_wchar(field) + 1 : 0 ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    uint32_t ncontents = 0 ;
    if (res->type != E_PARSER_TYPE_MODULE || !status_dependencies_load(&dep, res) || !dep.ncontents) {
        strbuf_free(&dep.sa) ;
        goto empty ;
    }
    ncontents = dep.ncontents ;

    if (!sbl_clean_string(&sa, dep.sa.s + dep.contents)) {
        strbuf_free(&dep.sa) ;
        log_dieu(LOG_EXIT_SYS, "clean string") ;
    }
    strbuf_free(&dep.sa) ;

    if (!service_graph_new(&graph, ncontents))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk(&graph, 0, 0, 0, REVERSE, &d, padding, S_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

        goto freed ;

    } else {

        sa.len = 0 ;
        uint32_t pos = 0 ;

        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {
            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            if (!sbl_add(&sa, name))
                log_die_nomem("strbuf") ;
        }

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }
    empty:
        if (GRAPH) {
            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        } else {
            info_display_empty() ;
        }

    freed:
        service_graph_destroy(&graph) ;
}

static uint8_t status_io_load(resolve_service_addon_io_t *io, resolve_service_t *res)
{
    if (!res->has_io)
        return 0 ;

    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_IO, io) ;
    uint8_t ok = resolve_read(w, res->sa.s + res->path.home, res->sa.s + res->name) > 0 ;
    free(w) ;

    return ok ;
}

static uint8_t status_dependencies_load(resolve_service_addon_dependencies_t *dep, resolve_service_t *res)
{
    if (!res->has_dependencies)
        return 0 ;

    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
    uint8_t ok = resolve_read(w, res->sa.s + res->path.home, res->sa.s + res->name) > 0 ;
    free(w) ;

    return ok ;
}

static uint8_t status_log_load(log_source_t *src, resolve_service_addon_io_t *io, resolve_service_t *res)
{
    if (res->type == E_PARSER_TYPE_MODULE || !status_io_load(io, res) || !io->fdout.destination)
        return 0 ;

    char const *name = res->sa.s + res->name ;
    char const *dest = io->sa.s + io->fdout.destination ;

    if (io->fdout.type == E_PARSER_IO_TYPE_66LOG) {

        // the logdir only exists once the service has run at least once
        if (scan_mode(dest, S_IFDIR) != 1)
            return 0 ;

        if (!log_source_logdir(src, name, dest))
            log_dieusys(LOG_EXIT_SYS, "read log directory of: ", name) ;

        return 1 ;
    }

    if (io->fdout.type == E_PARSER_IO_TYPE_FILE) {

        if (scan_mode(dest, S_IFREG) != 1)
            return 0 ;

        if (!log_source_file(src, name, dest))
            log_dieusys(LOG_EXIT_SYS, "read log file of: ", name) ;

        return 1 ;
    }

    return 0 ;
}

static void info_display_log(char const *field,resolve_service_t *res)
{
    log_flow() ;

    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    log_source_t src = LOG_SOURCE_ZERO ;
    char const *name = res->sa.s + res->name ;

    if (!status_log_load(&src, &io, res) || !src.nline)
        goto empty ;

    if (field) {

        if (res->logger) {

            char logname[strlen(name) + SS_LOG_SUFFIX_LEN + 1] ;
            auto_strings(logname, name, SS_LOG_SUFFIX) ;

            if (!ostream_fmt(ostream_1, "%s - '66 log %s' for more\n", logname, name))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

        } else if (!ostream_fmt(ostream_1, "'66 log %s' for more\n", name))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    size_t first = src.nline > STATUS_NLOG ? src.nline - STATUS_NLOG : 0 ;

    for (size_t i = first ; i < src.nline ; i++) {

        log_line_t *pl = &src.line[i] ;
        log_emit(src.data.s + pl->off, pl->len, pl->msgoff, pl->type, &pl->stamp, src.name, 0) ;
    }

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    log_source_free(&src) ;
    strbuf_free(&io.sa) ;
    return ;

    empty:
        log_source_free(&src) ;
        strbuf_free(&io.sa) ;
        info_display_empty() ;
}

static void write_value(void *ctx, size_t index, char const *label)
{
    (*fields_sv[index].render)(label, (resolve_service_t *)ctx) ;
}

static void info_status_all(void)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t f = REVERSE ? GRAPH_WANT_REQUIREDBY : GRAPH_WANT_DEPENDS ;
    uint32_t nservice = 0 , pos = 0, flag = f|GRAPH_COLLECT_PARSE ;

    if (!tree_graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = tree_graph_build_master(&graph, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_dieusys(LOG_EXIT_SYS, "find trees -- please make a bug report") ;

    _alloc_sbl_(stk, graph.g.nsort * SS_MAX_TREENAME) ;

    FOREACH_GRAPH_SORT(tree_graph_t, &graph, pos) {
        uint32_t index = graph.g.sort[pos] ;
        char *name = graph.g.sindex[index]->name ;
        if (!sbl_add(&stk, name)) {
            errno = EINVAL ;
            log_dieu(LOG_EXIT_SYS, "get the sorted list of trees") ;
        }
    }

    if (stk.len) {

        struct resolve_hash_tree_s *h ;
        service_graph_t sg = GRAPH_SERVICE_ZERO ;

        pos = 0 ;
        FOREACH_SBL(&stk, pos) {

            h = hash_search_tree(&graph.hres, stk.s + pos) ;
            if (h == NULL)
                log_dieusys(LOG_EXIT_ZERO, "get information of tree: ", stk.s + pos, " -- please make a bug report") ;

            if (h->tres.ncontents) {

                _alloc_sbl_(sv, strlen(h->tres.sa.s + h->tres.contents)) ;

                if (!sbl_clean_string(&sv, h->tres.sa.s + h->tres.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                /** A dependencies service can be on another tree,
                 * so used SS_MAX_SERVICE instead of sbl_count(&stk). */
                if (!service_graph_new(&sg, SS_MAX_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

                nservice = service_graph_build_list(&sg, sv.s, sv.len, pinfo, flag) ;

                if (!nservice && errno == EINVAL)
                    log_die(LOG_EXIT_USER, "build the graph -- please make a bug report") ;

                if (!ostream_fmt(ostream_1,"%s%s%s%s\n","In tree: ", log_color->info, h->tres.sa.s + h->tres.name, log_color->off))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
                if (!ostream_fmt(ostream_1,"%s\n","\\"))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

                depth_t d = info_graph_init() ;

                if (!service_info_walk(&sg, 0, h->tres.sa.s + h->tres.name, 0, REVERSE, &d, 0, S_STYLE, pinfo))
                    log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

                if (!ostream_puts(ostream_1,"\n"))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

                service_graph_destroy(&sg) ;
            }
        }

    } else {
        log_dieusys(LOG_EXIT_SYS, "find trees -- please make a bug report") ;
    }

    tree_graph_destroy(&graph) ;
}

static void info_status_one(char const *service, char const *select)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *keys[NFIELD], *labels[NFIELD] ;

    int r = service_is_g(service, STATE_FLAGS_ISPARSED) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", service, " -- please make a bug report") ;

    if (!r || r == STATE_FLAGS_FALSE)
        log_die(LOG_EXIT_SYS, "service: ", service, " is not parsed -- try to parse it using '66 parse ", service, "'") ;

    if (resolve_read(wres, pinfo->base.s, service) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", service) ;

    for (size_t i = 0 ; i < NFIELD ; i++) {
        keys[i] = fields_sv[i].key ;
        labels[i] = fields_sv[i].label ;
    }

    if (!select) {

        for (size_t i = 0 ; i < NFIELD ; i++) {

            if ((fields_sv[i].render == &info_display_contents && res.type != E_PARSER_TYPE_MODULE) ||
                fields_sv[i].render == &info_display_type ||
                fields_sv[i].render == &info_display_enabled ||
                fields_sv[i].render == &info_display_pid)
                continue ;

            if (sa.len && !auto_strbuf(&sa, ","))
                log_die_nomem("strbuf") ;

            if (!auto_strbuf(&sa, fields_sv[i].key))
                log_die_nomem("strbuf") ;
        }

        select = sa.s ;
    }

    info_fields_display(keys, labels, NFIELD, select, !NOFIELD, &write_value, &res) ;

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    resolve_free(wres) ;
}

static opt_t const opts_status[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",     .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name",  .arg = OPT_NONE,                             .help = "do not display the field name" },
    { .id = 'o',         .shortname = 'o', .longname = "options",  .arg = OPT_REQUIRED, .argname = "field,...", .help = "deprecated options, please use -f instead", .hidden = true },
    { .id = 'f',         .shortname = 'f', .longname = "field",    .arg = OPT_REQUIRED, .argname = "field,...", .help = "comma separated list of options" },
    { .id = 'g',         .shortname = 'g', .longname = "graph",    .arg = OPT_NONE,                             .help = "displays interdependences as graph" },
    { .id = 'r',         .shortname = 'r', .longname = "reverse",  .arg = OPT_NONE,                             .help = "reverse the interdependence graph" },
    { .id = 'd',         .shortname = 'd', .longname = "depth",    .arg = OPT_REQUIRED, .argname = "number",    .help = "limit the depth of interdependence graph recursion by depth" },
} ;

static char const *sta_select = 0 ;

static int on_status(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'n' :
            NOFIELD = 0 ;
            break ;

        case 'o' :
            log_1_warn("deprecated options, please use -f instead") ;
            attribute_fallthrough ;
        case 'f' :
            sta_select = arg ;
            break ;

        case 'g' :
            GRAPH = 1 ;
            break ;

        case 'r' :
            REVERSE = 1 ;
            break ;

        case 'd' :
            if (!u32_scan_strict(arg, &INFO_MAXDEPTH))
                log_die(LOG_EXIT_USER, "invalid depth value: ", arg) ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_status = {
    .name = "66 status",
    .help = "display services informations",
    .operands = "service",
    .opts = opts_status,
    .nopts = OPT_COUNT(opts_status),
    .on_option = &on_status,
    .fn = &ssexec_status,
    .epilog =
        "field:\n"
        "    name          status        description\n"
        "    type          source        tree\n"
        "    enabled       pid           depends\n"
        "    requiredby    contents      log\n"
        "\n"
        "See '66 resolve' for the parsed configuration of a service,\n"
        "and '66 log' to read, filter or follow its log.",
} ;

int ssexec_status(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into a local, then reset the static so a nested
     * re-dispatch of "status" starts clean. */
    char const *select = sta_select ;
    sta_select = 0 ;

    pinfo = info ;

    setlocale(LC_ALL, "");

    if(!strcmp(nl_langinfo(CODESET), "UTF-8"))
        S_STYLE = &graph_utf8;

    if (!argc)
        info_status_all() ;
    else
        info_status_one(*argv, select) ;

    return 0 ;
}
