/*
 * ssexec_tree_status.c
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

#include <string.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/types.h>
#include <wchar.h>
#include <unistd.h>//access
#include <errno.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/account.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/lexer.h>
#include <oblibs/stack.h>
#include <oblibs/hash.h>

#include <skalibs/sgetopt.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>

#include <66/info.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum_parser.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/state.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_name(char const *field,resolve_tree_t *res) ;
static void info_display_current(char const *field,resolve_tree_t *res) ;
static void info_display_enabled(char const *field,resolve_tree_t *res) ;
//static void info_display_init(char const *field,resolve_tree_t *res) ;
static void info_display_depends(char const *field,resolve_tree_t *res) ;
static void info_display_requiredby(char const *field,resolve_tree_t *res) ;
static void info_display_allow(char const *field,resolve_tree_t *res) ;
static void info_display_contents(char const *field,resolve_tree_t *res) ;
static void info_display_groups(char const *field,resolve_tree_t *res) ;
static info_graph_style *T_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;

info_opts_map_t const opts_tree_table[] =
{
    { .str = "name", .func = &info_display_name, .id = 0 },
    { .str = "current", .func = &info_display_current, .id = 1 },
    { .str = "enabled", .func = &info_display_enabled, .id = 2 },
//    { .str = "init", .func = &info_display_init, .id = 3 },
    { .str = "allowed", .func = &info_display_allow, .id = 3 },
    { .str = "groups", .func = &info_display_groups, .id = 4 },
    { .str = "depends", .func = &info_display_depends, .id = 5 },
    { .str = "requiredby", .func = &info_display_requiredby, .id = 6 },
    { .str = "contents", .func = &info_display_contents, .id = 7 },
    { .str = 0, .func = 0, .id = -1 }
} ;

#define MAXOPTS 9
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

static void info_display_name(char const *field, resolve_tree_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s",res->sa.s + res->name))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_current(char const *field,resolve_tree_t *res)
{
    int current = tree_iscurrent(pinfo->base.s, res->sa.s + res->name) ;
    if (current < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s", current ? log_color->valid : log_color->warning, current ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_enabled(char const *field,resolve_tree_t *res)
{
    int enabled = tree_isenabled(pinfo->base.s, res->sa.s + res->name) ;
    if (enabled < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",enabled ? log_color->valid : log_color->warning, enabled ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

/*
static void info_display_init(char const *field,resolve_tree_t *res)
{
     it not possible to write the resolve file of a tree at boot
       if the filesystem is ro. So consider tree as never initiated.
       ssexec_{start,stop,free,...} will deal with the live state of
       the services anyway.
    unsigned int init = tree_isinitialized(pinfo->base.s, treename) ;
    if (init == -1) log_dieu(LOG_EXIT_SYS, "resolve file of tree: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",init ? log_color->valid : log_color->warning, init ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;


}
*/

static void info_display_allow(char const *field, resolve_tree_t *res)
{

    _alloc_sa_(sa) ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (res->nallow) {

        if (!sastr_clean_string(&sa, res->sa.s + res->allow))
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
}

static void info_display_groups(char const *field, resolve_tree_t *res)
{
    _alloc_sa_(sa) ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (res->ngroups) {

        if (!sastr_clean_string(&sa, res->sa.s + res->groups))
            log_dieu(LOG_EXIT_SYS,"clean groups string") ;

        info_display_list(field,&sa) ;

    } else {

        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_depends(char const *field, resolve_tree_t *res)
{
    size_t padding = 1 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS, ntree = 0 ;
    _alloc_sa_(sa) ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->ndepends)
        goto empty ;

    if (!sastr_clean_string(&sa, res->sa.s + res->depends))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    ntree = tree_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!ntree && errno == EINVAL)
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!tree_info_walk(&graph, 0, 0, REVERSE, &d, padding, T_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        uint32_t pos = 0 ;
        sa.len = 0 ;
        FOREACH_GRAPH_SORT(tree_graph_t, &graph, pos) {
            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            if (!sastr_add_string(&sa, name))
                log_die_nomem("stralloc") ;
        }

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
        tree_graph_destroy(&graph) ;
}

static void info_display_requiredby(char const *field, resolve_tree_t *res)
{
    size_t padding = 1 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY, ntree = 0 ;
    _alloc_sa_(sa) ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->nrequiredby)
        goto empty ;

    if (!sastr_clean_string(&sa, res->sa.s + res->requiredby))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    ntree = tree_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!ntree && errno == EINVAL)
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!tree_info_walk(&graph, 0, 1, REVERSE, &d, padding, T_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        uint32_t pos = 0 ;
        sa.len = 0 ;
        FOREACH_GRAPH_SORT(tree_graph_t, &graph, pos) {
            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;

            if (!sastr_add_string(&sa, name))
                log_die_nomem("stralloc") ;
        }

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
        tree_graph_destroy(&graph) ;
}

static void info_display_contents(char const *field, resolve_tree_t *res)
{

    size_t padding = 1 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_COLLECT_PARSE, nservice = 0 ;
    _alloc_sa_(sa) ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->ncontents)
        goto empty ;

    if (!sastr_clean_string(&sa, res->sa.s + res->contents))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_dieusys(LOG_EXIT_SYS, "build the graph") ;

    if (!nservice)
        goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk(&graph, 0, res->sa.s + res->name, 0, REVERSE, &d, padding, T_STYLE, pinfo))
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
        service_graph_destroy(&graph) ;
}

static void info_display_all(const char *treename,int *what)
{

    unsigned int i = 0 ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (resolve_read_g(wres, pinfo->base.s, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

    for (; what[i] >= 0 ; i++) {
        unsigned int idx = what[i] ;
        (*opts_tree_table[idx].func)(fields[opts_tree_table[idx].id],&tres) ;
    }

}

static void info_parse_options(char const *str,int *what)
{
    size_t pos = 0 ;
    _alloc_stk_(stk, strlen(str) + 1) ;

    if (!lexer_trim_with_delim(&stk, str, DELIM))
        log_dieu(LOG_EXIT_SYS,"parse options") ;

    unsigned int nopts = 0 , old ;
    checkopts(stk.count) ;
    info_opts_map_t const *t ;

    FOREACH_STK(&stk, pos) {

        char *o = stk.s + pos ;
        t = opts_tree_table ;
        old = nopts ;
        for (; t->str; t++) {

            if (!strcmp(o,t->str))
                what[nopts++] = t->id ;
        }

        if (old == nopts)
            log_die(LOG_EXIT_SYS,"invalid option: ",o) ;
    }
}

int ssexec_tree_status(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    unsigned int legacy = 1 ;

    int what[MAXOPTS] = { 0 } ;

    pinfo = info ;

    char const *treename = 0 ;

    for (int i = 0 ; i < MAXOPTS ; i++)
        what[i] = -1 ;


    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "Name",
        "Current",
        "Enabled",
        //"Initialized",
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
                case 'd' :  if (!uint0_scan(l.arg, &INFO_MAXDEPTH)) log_usage(info->usage, "\n", info->help) ; break ;
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
            log_info("No tree exist yet") ;
            goto end ;
        }
    }

    if (treename) {

        if (!tree_isvalid(info->base.s, treename))
            log_dieusys(LOG_EXIT_SYS, "find tree: ", treename) ;

        if (!strcmp(treename, SS_MASTER + 1))
            log_die(LOG_EXIT_USER, "you can not view the status of the Master tree -- please use \'66 tree resolve Master\' instead") ;

        info_display_all(treename, what) ;

    } else {

        tree_graph_t graph = GRAPH_TREE_ZERO ;
        uint32_t flag = REVERSE ? GRAPH_WANT_REQUIREDBY : GRAPH_WANT_DEPENDS, ntree = 0, pos = 0 ;
        vertex_t *v = NULL ;

        if (!graph_new(&graph, SS_MAX_SERVICE))
            log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

        ntree = tree_graph_build_master(&graph, info, flag) ;

        if (!ntree && errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "build graph") ;

        if (ntree) {

            FOREACH_GRAPH_SORT(tree_graph_t, &graph, pos) {

                uint32_t index = graph.g.sort[pos] ;
                v = graph.g.sindex[index] ;
                char *name = v->name ;

                info_display_all(name, what) ;

                if (buffer_puts(buffer_1,"\n") == -1)
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            }

        } else {

            log_info("No tree exist yet") ;
            goto end ;
        }

        tree_graph_destroy(&graph) ;
    }

    if (buffer_flush(buffer_1) == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    end:

    return 0 ;
}
