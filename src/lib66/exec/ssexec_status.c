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
 * except according to the terms contained in the LICENSE file.
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
#include <stdbool.h>

#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/stream.h>
#include <oblibs/bits.h>

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
#include <66/selection.h>
#include <66/module.h>
#include <66/command.h>

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
static void info_display_deps(char const *field, resolve_service_t *res) ;
static void info_display_requiredby(char const *field, resolve_service_t *res) ;
static void info_display_contents(char const *field, resolve_service_t *res) ;
static void info_display_log(char const *field, resolve_service_t *res) ;

static info_graph_style *S_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;
static service_graph_t const *SG = 0 ;
static uint32_t SINDEX = SERVICE_GRAPH_NOINDEX ;
static struct resolve_hash_s *SH = 0 ;

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

static int status_record_load(service_status_t *st, resolve_service_t *res)
{
    int r = svc_status(res, st) ;
    if (r < 0)
        log_warnusys("read status of: ", res->sa.s + res->name) ;

    return r ;
}

static void info_get_status(resolve_service_t *res)
{
    service_status_t st = STATUS_ZERO ;
    int r = status_record_load(&st, res) ;
    char const *disen = !NOFIELD ? "" : res->enabled ? "enabled, " : "disabled, " ;

    if (!r && svc_status_effective(SH, &st) == STATUS_STATE_WAITING) {

        if (!ostream_fmt(ostream_1, "%s%s%s%s (never run)\n", disen, log_color->valid, status_state_to_string(STATUS_STATE_WAITING), log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        return ;
    }

    if (r <= 0) {

        if (NOFIELD && res->enabled && !ostream_puts(ostream_1, "enabled, "))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning, r < 0 ? "unreadable" : "None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        return ;
    }

    uint8_t estate = svc_status_effective(SH, &st) ;

    struct timespec now ;
    clock_now(&now) ;

    /* a oneshot or module reactor shown waiting after a firing: its state alone
     * never says whether its Execute already ran, only the date of its last run does. */
    char lastrun[INFO_DURATION_LEN + 18] = "" ;
    if (estate == STATUS_STATE_WAITING && res->type != E_PARSER_TYPE_CLASSIC) {

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

    if (!ostream_puts(ostream_1, disen))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    info_status_line(&st, estate, !NOFIELD, lastrun) ;
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

    if (status_record_load(&st, res) < 0)
        st = (service_status_t)STATUS_ZERO ;

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

static void level_of(bitset32_t *set, uint8_t requiredby, uint8_t contents)
{
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = SELECTION_SERVICE_POLICY_ZERO ;
    bitset32_t seed = bitset32_init(SS_MAX_SERVICE) ;

    if (contents) {

        service_graph_vertex_t const *v = service_graph_vertex(SG, SINDEX) ;
        uint32_t const *edge = service_graph_depends(SG, SINDEX) ;
        uint8_t const *origin = service_graph_dorigin(SG, SINDEX) ;

        *set = bitset32_init(SS_MAX_SERVICE) ;

        for (uint32_t pos = 0 ; pos < v->ndepends ; pos++)
            if (origin[pos] & SERVICE_GRAPH_EDGE_CONTENTS)
                bitset32_set(set, edge[pos]) ;

        return ;
    }

    policy.propagate = requiredby ? SELECTION_SERVICE_PROPAGATE_REQUIREDBY : SELECTION_SERVICE_PROPAGATE_DEPENDS ;
    bitset32_set(&seed, SINDEX) ;

    selection_service_propagate(&sel, SG, &seed, &policy) ;
    selection_service_remove(&sel, SINDEX) ;

    *set = sel.member ;
}

static void display_level(char const *field, uint8_t requiredby, uint8_t contents)
{
    log_flow() ;

    size_t padding = field ? info_length_from_wchar(field) + 1 : 0 ;
    uint32_t list[service_graph_nvertex(SG) + 1] ;
    uint32_t count ;
    bitset32_t set ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    level_of(&set, requiredby, contents) ;

    count = service_info_order(list, SG, &set, requiredby) ;

    if (!count) {

        if (GRAPH) {
            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        } else {
            info_display_empty() ;
        }

        return ;
    }

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk_set(SG, &set, requiredby, REVERSE, &d, padding, S_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the list of: ", field ? field : "the service") ;

        return ;
    }

    for (uint32_t pos = 0 ; pos < count ; pos++)
        if (!sbl_add(&sa, service_graph_name(SG, list[pos])))
            log_die_nomem("strbuf") ;

    if (REVERSE && !sbl_reverse(&sa))
        log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

    info_display_list(field, &sa) ;
}

static void info_display_requiredby(char const *field, resolve_service_t *res)
{
    log_flow() ;

    (void)res ;
    display_level(field, 1, 0) ;
}

static void info_display_deps(char const *field, resolve_service_t *res)
{
    log_flow() ;

    (void)res ;
    display_level(field, 0, 0) ;
}

static void info_display_contents(char const *field, resolve_service_t *res)
{
    log_flow() ;

    (void)res ;
    display_level(field, 0, 1) ;
}

static uint8_t status_log_load(log_source_t *src, resolve_service_addon_io_t const *io, resolve_service_t *res)
{
    if (res->type == E_PARSER_TYPE_MODULE || !res->has_io || !io->fdout.destination)
        return 0 ;

    char const *name = res->sa.s + res->name ;
    char const *dest = io->sa.s + io->fdout.destination ;

    if (io->fdout.type == E_PARSER_IO_TYPE_66LOG) {

        // the logdir only exists once the service has run at least once
        ssize_t r = scan_mode(dest, FLAGS_DIR) ;
        if (!r && errno != ENOENT)
            log_dieusys(LOG_EXIT_SYS, "check log directory of: ", name) ;

        if (r != 1)
            return 0 ;

        if (!log_source_logdir(src, name, dest))
            log_dieusys(LOG_EXIT_SYS, "read log directory of: ", name) ;

        return 1 ;
    }

    if (io->fdout.type == E_PARSER_IO_TYPE_FILE) {

        ssize_t r = scan_mode(dest, FLAGS_REG) ;
        if (!r && errno != ENOENT)
            log_dieusys(LOG_EXIT_SYS, "check log file of: ", name) ;

        if (r != 1)
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

    resolve_service_addon_io_t const *io = &SH->io ;
    log_source_t src = LOG_SOURCE_ZERO ;
    char const *name = res->sa.s + res->name ;

    if (!status_log_load(&src, io, res) || !src.nline)
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
    return ;

    empty:
        log_source_free(&src) ;
        info_display_empty() ;
}

static void write_value(void *ctx, size_t index, char const *label)
{
    (*fields_sv[index].render)(label, (resolve_service_t *)ctx) ;
}

static void info_status_all(void)
{
    log_flow() ;

    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = REVERSE ? GRAPH_WANT_REQUIREDBY : GRAPH_WANT_DEPENDS ;
    uint32_t ntree = 0, pos = 0 ;

    if (!tree_graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    ntree = tree_graph_build_master(&graph, pinfo, flag) ;

    if (!ntree)
        log_dieusys(LOG_EXIT_SYS, "find trees -- please make a bug report") ;

    FOREACH_GRAPH_SORT(tree_graph_t, &graph, pos) {

        uint32_t index = graph.g.sort[pos] ;
        char const *treename = graph.g.sindex[index]->name ;
        struct resolve_hash_tree_s *h = hash_search_tree(&graph.hres, treename) ;
        size_t spos = 0 ;
        bitset32_t set = bitset32_init(SS_MAX_SERVICE) ;
        _cleanup_strbuf_ strbuf sv = STRBUF_ZERO ;

        if (h == NULL)
            log_dieusys(LOG_EXIT_ZERO, "get information of tree: ", treename, " -- please make a bug report") ;

        if (!h->tres.ncontents)
            continue ;

        if (!sbl_clean_string(&sv, h->tres.sa.s + h->tres.contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        FOREACH_SBL(&sv, spos) {

            uint32_t v = service_graph_find(SG, sv.s + spos) ;

            if (v != SERVICE_GRAPH_NOINDEX)
                bitset32_set(&set, v) ;
        }

        if (!ostream_fmt(ostream_1,"%s%s%s%s\n","In tree: ", log_color->info, treename, log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk_set(SG, &set, 0, REVERSE, &d, 0, S_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

        if (!ostream_puts(ostream_1,"\n"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }

    tree_graph_destroy(&graph) ;
}

static void info_status_one(char const *select)
{
    resolve_service_t *res = &SH->res ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *keys[NFIELD], *labels[NFIELD] ;

    for (size_t i = 0 ; i < NFIELD ; i++) {
        keys[i] = fields_sv[i].key ;
        labels[i] = fields_sv[i].label ;
    }

    if (!select) {

        for (size_t i = 0 ; i < NFIELD ; i++) {

            if ((fields_sv[i].render == &info_display_contents && res->type != E_PARSER_TYPE_MODULE) ||
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

    info_fields_display(keys, labels, NFIELD, select, !NOFIELD, &write_value, res) ;

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void prepare_absent(service_graph_t *g, ssexec_t *info)
{
    int n = 0 ;
    size_t pos = 0 ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(g) ; index++) {

        service_graph_vertex_t const *v = service_graph_vertex(g, index) ;
        uint8_t const *origin = service_graph_rorigin(g, index) ;
        int reached = 0 ;

        if (v->state != SERVICE_GRAPH_STATE_ABSENT)
            continue ;

        for (uint32_t e = 0 ; e < v->nrequiredby && !reached ; e++)
            reached = (origin[e] & SERVICE_GRAPH_EDGE_FROM) || ((origin[e] & SERVICE_GRAPH_EDGE_DECLARED) && !(origin[e] & SERVICE_GRAPH_EDGE_REQUIREDBY)) ;

        if (!reached)
            continue ;

        int r = service_frontend_path(&sa, service_graph_name(g, index), info->owner, 0, exclude, 2) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", service_graph_name(g, index)) ;

        if (r && !sbl_add(&names, service_graph_name(g, index)))
            log_die_nomem("strbuf") ;
    }

    if (!names.len)
        return ;

    char const *argv[sbl_count(&names) + 1] ;

    FOREACH_SBL(&names, pos)
        argv[n++] = names.s + pos ;
    argv[n] = 0 ;

    command_parse_missing(g, argv, n, info, 0) ;
}

static void refuse_gone(service_graph_t const *g, uint32_t index, strbuf *frontends)
{
    int r ;
    char const *name = service_graph_name(g, index) ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    r = service_graph_frontend_gone(g, index, frontends) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", name) ;

    if (!r)
        return ;

    if (service_graph_invalid_message(&msg, g, index, true) < 0)
        log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", name) ;

    log_die(LOG_EXIT_USER, msg.s) ;
}

static int select_named(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends)
{
    int r ;
    uint32_t index ;
    struct resolve_hash_s *h ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_NONE, false, false, false) ;

    if (!command_argv_to_select(sel, d, argv, argc, info, &policy, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID))
        return 0 ;

    for (index = 0 ; index < service_graph_nvertex(&d->g) ; index++) {

        if (!bitset32_isvalid(&sel->named, index))
            continue ;

        refuse_gone(&d->g, index, frontends) ;

        r = service_graph_resolve_read(&d->store, index, SERVICE_READ_IO, &h) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read the service: ", service_graph_name(&d->g, index)) ;

        if (!r) {
            if (d->reloaded)
                log_dieu(LOG_EXIT_SYS, "read the services of the selection -- the system changed under the command") ;
            return 0 ;
        }
    }

    return 1 ;
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

    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;

    pinfo = info ;

    setlocale(LC_ALL, "");

    if(!strcmp(nl_langinfo(CODESET), "UTF-8"))
        S_STYLE = &graph_utf8;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    if (!argc) {

        prepare_absent(&d.g, info) ;

        if (service_graph_warn_invalid(&d.g, &frontends, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

        SG = &d.g ;
        info_status_all() ;

    } else {

        command_parse_missing(&d.g, argv, argc, info, 0) ;

        while (!select_named(&sel, &d, argv, argc, info, &frontends))
            command_graph_reload(&d, info, false) ;

        if (service_graph_warn_invalid(&d.g, &frontends, &sel.named) < 0)
            log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

        uint32_t pos, index ;

        FOREACH_SELECTION(&sel, pos, index) {

            if (!bitset32_isvalid(&sel.named, index))
                continue ;

            if (service_graph_resolve_read(&d.store, index, SERVICE_READ_IO, &SH) <= 0)
                log_dieusys(LOG_EXIT_SYS, "read the service: ", service_graph_name(&d.g, index)) ;

            SG = &d.g ;
            SINDEX = index ;
            info_status_one(select) ;
        }
    }

    SG = 0 ;
    SH = 0 ;
    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;
    strbuf_free(&frontends) ;

    return 0 ;
}
