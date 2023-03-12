/*
 * ssexec_tree_status.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <locale.h>
#include <langinfo.h>
#include <sys/types.h>
#include <wchar.h>
#include <unistd.h>//access
#include <stdio.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/sgetopt.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>

#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_name(char const *field,char const *treename) ;
static void info_display_current(char const *field,char const *treename) ;
static void info_display_enabled(char const *field,char const *treename) ;
static void info_display_init(char const *field,char const *treename) ;
static void info_display_depends(char const *field,char const *treename) ;
static void info_display_requiredby(char const *field,char const *treename) ;
static void info_display_allow(char const *field,char const *treename) ;
static void info_display_contents(char const *field,char const *treename) ;
static void info_display_groups(char const *field,char const *treename) ;
static info_graph_style *T_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;

info_opts_map_t const opts_tree_table[] =
{
    { .str = "name", .func = &info_display_name, .id = 0 },
    { .str = "current", .func = &info_display_current, .id = 1 },
    { .str = "enabled", .func = &info_display_enabled, .id = 2 },
    { .str = "init", .func = &info_display_init, .id = 3 },
    { .str = "allowed", .func = &info_display_allow, .id = 4 },
    { .str = "groups", .func = &info_display_groups, .id = 5 },
    { .str = "depends", .func = &info_display_depends, .id = 6 },
    { .str = "requiredby", .func = &info_display_requiredby, .id = 7 },
    { .str = "contents", .func = &info_display_contents, .id = 8 },
    { .str = 0, .func = 0, .id = -1 }
} ;

#define MAXOPTS 10
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

static void info_display_name(char const *field, char const *treename)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s",treename))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_current(char const *field,char const *treename)
{
    int current = tree_iscurrent(pinfo->base.s, treename) ;
    if (current < 0)
        log_dieu(LOG_EXIT_ZERO, "read resolve file of: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s", current ? log_color->valid : log_color->warning, current ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_enabled(char const *field,char const *treename)
{
    int enabled = tree_isenabled(pinfo->base.s, treename) ;
    if (enabled < 0)
        log_dieu(LOG_EXIT_ZERO, "read resolve file of: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",enabled ? log_color->valid : log_color->warning, enabled ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_init(char const *field,char const *treename)
{
    unsigned int init = tree_isinitialized(pinfo->base.s, treename) ;
    if (init == -1) log_dieu(LOG_EXIT_SYS, "resolve file of tree: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",init ? log_color->valid : log_color->warning, init ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

}

static void info_display_allow(char const *field, char const *treename)
{

    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (!resolve_read_g(wres, pinfo->base.s, treename))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of tree: ", treename) ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (tres.nallow) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.allow))
            log_dieu(LOG_EXIT_SYS,"clean groups string") ;

        if (!sa.len)
            goto empty ;

        size_t len = sa.len, pos = 0 ;
        char t[len + 1] ;

        sastr_to_char(t, &sa) ;

        sa.len = 0 ;

        for (; pos < len ; pos += strlen(t + pos) + 1) {

            char *suid = t + pos ;
            uid_t uid = 0 ;
            if (!uid0_scan(suid, &uid))
                log_dieusys(LOG_EXIT_SYS,"get uid of: ",suid) ;
            if (pos)
                if (!stralloc_cats(&sa," ")) log_die_nomem("stralloc") ;
            if (!get_namebyuid(uid,&sa))
                log_dieusys(LOG_EXIT_SYS, "get name of uid: ", suid) ;
        }

        if (!stralloc_0(&sa)) log_die_nomem("stralloc") ;
        if (!sastr_rebuild_in_oneline(&sa)) log_dieu(LOG_EXIT_SYS,"rebuild list") ;

        if (!stralloc_0(&sa)) log_die_nomem("stralloc") ;

        info_display_list(field,&sa) ;

    } else {

        empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }

    resolve_free(wres) ;
    stralloc_free(&sa) ;

}

static void info_display_groups(char const *field, char const *treename)
{
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (!resolve_read_g(wres, pinfo->base.s, treename))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", treename) ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (tres.ngroups) {

        if (!sastr_clean_string(&sa, tres.sa.s + tres.groups))
            log_dieu(LOG_EXIT_SYS,"clean groups string") ;

        info_display_list(field,&sa) ;

    } else {

        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }

    resolve_free(wres) ;
    stralloc_free(&sa) ;

}

static void info_display_depends(char const *field, char const *treename)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    graph_build_tree(&graph, pinfo->base.s, E_RESOLVE_TREE_MASTER_CONTENTS) ;

    r = graph_matrix_get_edge_g_sorted_sa(&sa, &graph, treename, 0, 0) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

    if (!r || !sa.len)
        goto empty ;

    if (GRAPH) {
        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, treename, 0, &info_graph_display_tree, 0, REVERSE, &d, padding, T_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        if (REVERSE)
            if (!sastr_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }
    empty:
        if (GRAPH) {

            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        stralloc_free(&sa) ;
}

static void info_display_requiredby(char const *field, char const *treename)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    graph_build_tree(&graph, pinfo->base.s, E_RESOLVE_TREE_MASTER_CONTENTS) ;

    r = graph_matrix_get_edge_g_sorted_sa(&sa, &graph, treename, 1, 0) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

    if (!r || !sa.len)
        goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, treename, 0, &info_graph_display_tree, 1, REVERSE, &d, padding, T_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        if (REVERSE)
            if (!sastr_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:
        if (GRAPH) {

            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        stralloc_free(&sa) ;
}

static void info_display_contents(char const *field, char const *treename)
{

    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    graph_build_service(&graph, ares, &areslen, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    if (!areslen)
        goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, 0, treename, &info_graph_display_service, 0, REVERSE, &d, padding, T_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        stralloc sa = STRALLOC_ZERO ;

        if (!resolve_get_field_tosa_g(&sa, pinfo->base.s, treename, DATA_TREE, E_RESOLVE_TREE_CONTENTS))
            log_dieu(LOG_EXIT_SYS, "get tree services list of: ", treename) ;

        if (!sa.len)
            goto empty ;

        if (REVERSE)
            if (!sastr_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:

        if (GRAPH) {

            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
}

static void info_display_all(char const *treename,int *what)
{

    unsigned int i = 0 ;
    for (; what[i] >= 0 ; i++) {

        unsigned int idx = what[i] ;
        (*opts_tree_table[idx].func)(fields[opts_tree_table[idx].id],treename) ;
    }

}

static void info_parse_options(char const *str,int *what)
{
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!sastr_clean_string_wdelim(&sa,str,DELIM)) log_dieu(LOG_EXIT_SYS,"parse options") ;
    unsigned int n = sastr_len(&sa), nopts = 0 , old ;
    checkopts(n) ;
    info_opts_map_t const *t ;

    for (;pos < sa.len; pos += strlen(sa.s + pos) + 1) {

        char *o = sa.s + pos ;
        t = opts_tree_table ;
        old = nopts ;
        for (; t->str; t++) {

            if (obstr_equal(o,t->str))
                what[nopts++] = t->id ;
        }

        if (old == nopts)
            log_die(LOG_EXIT_SYS,"invalid option: ",o) ;
    }

    stralloc_free(&sa) ;
}

int ssexec_tree_status(int argc, char const *const *argv, ssexec_t *info)
{
    unsigned int legacy = 1 ;

    size_t pos = 0 ;
    int what[MAXOPTS] = { 0 } ;
    stralloc sa = STRALLOC_ZERO ;

    pinfo = info ;

    char const *treename = 0 ;

    for (int i = 0 ; i < MAXOPTS ; i++)
        what[i] = -1 ;


    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "Name",
        "Current",
        "Enabled",
        "Initialized",
        "Allowed",
        "Groups",
        "Depends",
        "Required by",
        "Contents" } ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_TREE_STATUS, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'n' :  NOFIELD = 0 ; break ;
                case 'o' :  legacy = 0 ; info_parse_options(l.arg,what) ; break ;
                case 'g' :  GRAPH = 1 ; break ;
                case 'r' :  REVERSE = 1 ; break ;
                case 'd' :  if (!uint0_scan(l.arg, &MAXDEPTH)) log_usage(info->usage, "\n", info->help) ; break ;
                default :   log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argv[0]) treename = argv[0] ;

    if (legacy) {

        unsigned int i = 0 ;
        for (; i < MAXOPTS - 1 ; i++)
            what[i] = i ;

        what[i] = -1 ;
    }


    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    setlocale(LC_ALL, "");

    if(!str_diff(nl_langinfo(CODESET), "UTF-8"))
        T_STYLE = &graph_utf8;

    {
        /** should never happens as long as we have
         * a default created */
        char src[info->base.len + SS_SYSTEM_LEN + 1] ;

        auto_strings(src, info->base.s, SS_SYSTEM) ;

        if (!scan_mode(src, S_IFDIR)) {
            log_info("no tree exist yet") ;
            goto freed ;
        }
    }

    if (treename) {

        if (!tree_isvalid(info->base.s, treename))
            log_dieusys(LOG_EXIT_SYS, "find tree: ", treename) ;

        info_display_all(treename, what) ;

    } else {

        graph_t graph = GRAPH_ZERO ;

        graph_build_tree(&graph, pinfo->base.s, E_RESOLVE_TREE_MASTER_CONTENTS) ;

        if (!graph_matrix_sort_tosa(&sa, &graph))
            log_dieu(LOG_EXIT_SYS, "get the sorted list of trees") ;

        graph_free_all(&graph) ;

        if (sa.len) {

            FOREACH_SASTR(&sa, pos) {

                info_display_all(sa.s + pos, what) ;

                if (buffer_puts(buffer_1,"\n") == -1)
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            }

        } else {

            log_info("no tree exist yet") ;
            goto freed ;
        }
    }

    if (buffer_flush(buffer_1) == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    freed:

    stralloc_free(&sa) ;

    return 0 ;
}
