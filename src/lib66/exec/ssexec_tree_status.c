/*
 * ssexec_tree_status.c
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

#include <sys/stat.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/types.h>
#include <wchar.h>
#include <unistd.h>//access
#include <errno.h>

#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/account.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/lexer.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>
#include <oblibs/stream.h>

#include <66/info.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
static unsigned int LEGACY = 1 ;

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

/* One row per displayable field, in display order. The single source of truth:
 * key is what -o selects, label is the printed field name, render does the work. */

typedef struct tree_field_s tree_field_t ;
struct tree_field_s {
    char const *key ;
    char const *label ;
    void (*render)(char const *field, resolve_tree_t *res) ;
} ;

static tree_field_t const fields_tree[] = {
    { "name",       "Name",        &info_display_name },
    { "current",    "Current",     &info_display_current },
    { "enabled",    "Enabled",     &info_display_enabled },
//  { "init",       "Initialized", &info_display_init },
    { "allowed",    "Allowed",     &info_display_allow },
    { "groups",     "Groups",      &info_display_groups },
    { "depends",    "Depends",     &info_display_depends },
    { "requiredby", "Required by", &info_display_requiredby },
    { "contents",   "Contents",    &info_display_contents },
} ;

#define NFIELD OPT_COUNT(fields_tree)
#define DELIM ','

static int WHAT[NFIELD + 1] = { -1 } ;

static void info_display_name(char const *field, resolve_tree_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (!ostream_puts(ostream_1,res->sa.s + res->name))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_current(char const *field,resolve_tree_t *res)
{
    int current = tree_iscurrent(pinfo->base.s, res->sa.s + res->name) ;
    if (current < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!ostream_fmt(ostream_1,"%s%s%s", current ? log_color->valid : log_color->warning, current ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_enabled(char const *field,resolve_tree_t *res)
{
    int enabled = tree_isenabled(pinfo->base.s, res->sa.s + res->name) ;
    if (enabled < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!ostream_fmt(ostream_1,"%s%s%s",enabled ? log_color->valid : log_color->warning, enabled ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (!ostream_putflush(ostream_1, "\n", 1))
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
    if (!ostream_fmt(ostream_1,"%s%s%s",init ? log_color->valid : log_color->warning, init ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;


}
*/

static void info_display_allow(char const *field, resolve_tree_t *res)
{

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (res->nallow) {

        if (!sbl_clean_string(&sa, res->sa.s + res->allow))
            log_dieu(LOG_EXIT_SYS,"clean groups string") ;

        if (!sa.len)
            goto empty ;

        size_t len = sa.len, pos = 0 ;
        char t[len + 1] ;

        sbl_to_char(t, &sa) ;

        sa.len = 0 ;

        for (; pos < len ; pos += strlen(t + pos) + 1) {

            char *suid = t + pos ;
            uid_t uid = 0 ;
            if (!uid_parse_strict(suid, &uid))
                log_dieusys(LOG_EXIT_SYS,"get uid of: ",suid) ;
            if (pos)
                if (!strbuf_cats(&sa," ")) log_die_nomem("strbuf") ;
            if (!get_namebyuid(uid,&sa))
                log_dieusys(LOG_EXIT_SYS, "get name of uid: ", suid) ;
        }

        if (!strbuf_terminate(&sa)) log_die_nomem("strbuf") ;
        if (!sbl_rebuild_oneline(&sa)) log_dieu(LOG_EXIT_SYS,"rebuild list") ;

        if (!strbuf_terminate(&sa)) log_die_nomem("strbuf") ;

        info_display_list(field,&sa) ;

    } else {

        empty:
        if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_groups(char const *field, resolve_tree_t *res)
{
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (NOFIELD)
        info_display_field_name(field) ;

    if (res->ngroups) {

        if (!sbl_clean_string(&sa, res->sa.s + res->groups))
            log_dieu(LOG_EXIT_SYS,"clean groups string") ;

        info_display_list(field,&sa) ;

    } else {

        if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_depends(char const *field, resolve_tree_t *res)
{
    size_t padding = 1 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS, ntree = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->ndepends)
        goto empty ;

    if (!sbl_clean_string(&sa, res->sa.s + res->depends))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!tree_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    ntree = tree_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!ntree && errno == EINVAL)
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
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

            if (!sbl_add(&sa, name))
                log_die_nomem("strbuf") ;
        }

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }
    empty:
        if (GRAPH) {

            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
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
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->nrequiredby)
        goto empty ;

    if (!sbl_clean_string(&sa, res->sa.s + res->requiredby))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!tree_graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    ntree = tree_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!ntree && errno == EINVAL)
        log_dieu(LOG_EXIT_SYS, "build the graph") ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
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

            if (!sbl_add(&sa, name))
                log_die_nomem("strbuf") ;
        }

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:
        if (GRAPH) {

            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
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
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->ncontents)
        goto empty ;

    if (!sbl_clean_string(&sa, res->sa.s + res->contents))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!service_graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, pinfo, flag) ;

    if (!nservice && errno == EINVAL)
        log_dieusys(LOG_EXIT_SYS, "build the graph") ;

    if (!nservice)
        goto empty ;

    if (GRAPH) {

        if (!ostream_fmt(ostream_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!service_info_walk(&graph, 0, res->sa.s + res->name, 0, REVERSE, &d, padding, T_STYLE, pinfo))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;

    } else {

        if (REVERSE)
            if (!sbl_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }

    empty:

        if (GRAPH) {

            if (!ostream_fmt(ostream_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!ostream_fmt(ostream_1,"%*s%s%s%s%s\n",(int)padding, "", T_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!ostream_fmt(ostream_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
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
        (*fields_tree[idx].render)(fields[idx],&tres) ;
    }

    resolve_free(wres) ;
}

static void info_parse_options(char const *str,int *what)
{
    size_t pos = 0 ;
    unsigned int nopts = 0 ;
    _alloc_sbl_(stk, strlen(str) + 1) ;

    for (size_t i = 0 ; i < NFIELD + 1 ; i++)
        what[i] = -1 ;

    if (!lexer_trim_with_delim(&stk, str, DELIM))
        log_dieu(LOG_EXIT_SYS,"parse options") ;

    if (sbl_count(&stk) > NFIELD)
        log_die(LOG_EXIT_USER, "too many options") ;

    FOREACH_SBL(&stk, pos) {

        char *o = stk.s + pos ;
        size_t i = 0 ;

        for (; i < NFIELD ; i++)
            if (!strcmp(o, fields_tree[i].key))
                break ;

        if (i == NFIELD)
            log_die(LOG_EXIT_SYS,"invalid option: ",o) ;

        what[nopts++] = i ;
    }
}

int on_tree_status(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'n' :  NOFIELD = 0 ; break ;
        case 'o' :  LEGACY = 0 ; info_parse_options(arg, WHAT) ; break ;
        case 'g' :  GRAPH = 1 ; break ;
        case 'r' :  REVERSE = 1 ; break ;
        case 'd' :  if (!u32_scan_strict(arg, &INFO_MAXDEPTH)) log_die(LOG_EXIT_USER, "invalid depth value: ", arg) ; break ;
    }

    return 0 ;
}

int ssexec_tree_status(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int *what = WHAT ;

    pinfo = info ;

    char const *treename = 0 ;

    char buf[NFIELD][INFO_FIELD_MAXLEN] ;
    for (size_t i = 0 ; i < NFIELD ; i++)
        memcpy(buf[i], fields_tree[i].label, strlen(fields_tree[i].label) + 1) ;

    if (argc >= 1) treename = argv[0] ;

    if (LEGACY) {

        size_t i = 0 ;
        for (; i < NFIELD ; i++)
            what[i] = i ;

        what[i] = -1 ;
    }

    info_field_align(buf,fields,field_suffix,NFIELD) ;

    setlocale(LC_ALL, "");

    if(!strcmp(nl_langinfo(CODESET), "UTF-8"))
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

        if (!tree_graph_new(&graph, SS_MAX_SERVICE))
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

                if (!ostream_puts(ostream_1,"\n"))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            }

        } else {

            log_info("No tree exist yet") ;
            goto end ;
        }

        tree_graph_destroy(&graph) ;
    }

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    end:

    return 0 ;
}

