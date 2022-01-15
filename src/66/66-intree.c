/*
 * 66-intree.c
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
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
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
#include <66/backup.h>
#include <66/graph.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;

static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static stralloc src = STRALLOC_ZERO ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_name(char const *field,char const *treename) ;
static void info_display_current(char const *field,char const *treename) ;
static void info_display_enabled(char const *field,char const *treename) ;
static void info_display_init(char const *field,char const *treename) ;
static void info_display_depends(char const *field,char const *treename) ;
static void info_display_requiredby(char const *field,char const *treename) ;
static void info_display_allow(char const *field,char const *treename) ;
static void info_display_symlink(char const *field,char const *treename) ;
static void info_display_contents(char const *field,char const *treename) ;
static void info_display_groups(char const *field,char const *treename) ;
info_graph_style *STYLE = &graph_default ;

info_opts_map_t const opts_tree_table[] =
{
    { .str = "name", .func = &info_display_name, .id = 0 },
    { .str = "current", .func = &info_display_current, .id = 1 },
    { .str = "enabled", .func = &info_display_enabled, .id = 2 },
    { .str = "init", .func = &info_display_init, .id = 3 },
    { .str = "allowed", .func = &info_display_allow, .id = 4 },
    { .str = "symlinks", .func = &info_display_symlink, .id = 5 },
    { .str = "groups", .func = &info_display_groups, .id = 6 },
    { .str = "depends", .func = &info_display_depends, .id = 7 },
    { .str = "requiredby", .func = &info_display_requiredby, .id = 8 },
    { .str = "contents", .func = &info_display_contents, .id = 9 },
    { .str = 0, .func = 0, .id = -1 }
} ;

#define MAXOPTS 11
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

#define USAGE "66-intree [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -n ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -n: do not display the names of fields\n"
"   -o: comma separated list of field to display\n"
"   -g: displays the contents field as graph\n"
"   -d: limit the depth of the contents field recursion\n"
"   -r: reverse the contents field\n"
"\n"
"valid fields for -o options are:\n"
"\n"
"   name: displays the name of the tree\n"
"   current: displays a boolean value of the current state\n"
"   enabled: displays a boolean value of the enable state\n"
"   init: displays a boolean value of the initialization state\n"
"   depends: displays the list of tree(s) started before\n"
"   requiredby: displays the list of tree(s) started after\n"
"   allowed: displays a list of allowed user to use the tree\n"
"   symlinks: displays the target of tree's symlinks\n"
"   contents: displays the contents of the tree\n"
;

    log_info(USAGE,"\n",help) ;
}

static void info_display_name(char const *field, char const *treename)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s",treename))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_init(char const *field,char const *treename)
{
    unsigned int init = 0 ;
    int r = set_livedir(&live) ;
    if (!r) log_die_nomem("stralloc") ;
    if (r == -1) log_die(LOG_EXIT_SYS,"live: ",live.s," must be an absolute path") ;

    init = tree_isinitialized(live.s, treename, OWNER) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",init ? log_color->valid : log_color->warning, init ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

}

static void info_display_current(char const *field,char const *treename)
{
    int current = tree_iscurrent(base.s, treename) ;
    if (current < 0)
        log_dieu(LOG_EXIT_ZERO, "read resolve file of: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s", current ? log_color->blink : log_color->warning, current ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_enabled(char const *field,char const *treename)
{
    int enabled = tree_isenabled(base.s, treename) ;
    if (enabled < 0)
        log_dieu(LOG_EXIT_ZERO, "read resolve file of: ", treename) ;

    if (NOFIELD) info_display_field_name(field) ;
    if (!bprintf(buffer_1,"%s%s%s",enabled ? log_color->valid : log_color->warning, enabled ? "yes":"no",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_depends(char const *field, char const *treename)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!graph_build_g(&graph, base.s, treename, DATA_TREE))
        log_dieu(LOG_EXIT_SYS,"build the graph") ;

    r = graph_matrix_get_edge_g_sorted(&sa, &graph, treename, 0) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

    if (!r || !sa.len)
        goto empty ;

    if (GRAPH) {
        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, treename, treename, &info_graph_display_tree, 0, REVERSE, &d, padding, STYLE))
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

            if (!bprintf(buffer_1,"%s\n","/"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", STYLE->last, log_color->warning,"None",log_color->off))
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

    if (!graph_build_g(&graph, base.s, treename, DATA_TREE))
        log_dieu(LOG_EXIT_SYS,"build the graph") ;

    r = graph_matrix_get_edge_g_sorted(&sa, &graph, treename, 1) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

    if (!r || !sa.len)
        goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, treename, treename, &info_graph_display_tree, 1, REVERSE, &d, padding, STYLE))
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

            if (!bprintf(buffer_1,"%s\n","/"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        } else {

            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        stralloc_free(&sa) ;
}

static void info_display_allow(char const *field, char const *treename)
{
    if (NOFIELD) info_display_field_name(field) ;
    size_t treenamelen = strlen(treename) , pos ;
    stralloc sa = STRALLOC_ZERO ;
    stralloc salist = STRALLOC_ZERO ;
    char const *exclude[1] = { 0 } ;

    char tmp[src.len + treenamelen + SS_RULES_LEN + 1 ] ;
    // force to close the string
    auto_strings(tmp,src.s) ;
    auto_string_from(tmp,src.len,treename,SS_RULES) ;

    if (!sastr_dir_get(&sa,tmp,exclude,S_IFREG))
        log_dieusys(LOG_EXIT_SYS,"get permissions of tree at: ",tmp) ;

    for (pos = 0 ; pos < sa.len ; pos += strlen(sa.s + pos) + 1)
    {
        char *suid = sa.s + pos ;
        uid_t uid = 0 ;
        if (!uid0_scan(suid, &uid))
            log_dieusys(LOG_EXIT_SYS,"get uid of: ",suid) ;
        if (pos)
            if (!stralloc_cats(&salist," ")) log_die_nomem("stralloc") ;
        if (!get_namebyuid(uid,&salist))
            log_dieusys(LOG_EXIT_SYS,"get name of uid: ",suid) ;
    }
    if (!stralloc_0(&salist)) log_die_nomem("stralloc") ;
    if (!sastr_rebuild_in_oneline(&salist)) log_dieu(LOG_EXIT_SYS,"rebuild list: ",salist.s) ;
    if (!stralloc_0(&salist)) log_die_nomem("stralloc") ;
    info_display_list(field,&salist) ;

    stralloc_free(&sa) ;
    stralloc_free(&salist) ;
}

static void info_display_symlink(char const *field, char const *treename)
{
    if (NOFIELD) info_display_field_name(field) ;
    ssexec_t info = SSEXEC_ZERO ;
    if (!auto_stra(&info.treename,treename)) log_die_nomem("stralloc") ;
    if (!auto_stra(&info.base,base.s)) log_die_nomem("stralloc") ;
    int db , svc ;
    size_t typelen ;
    char type[UINT_FMT] ;
    typelen = uint_fmt(type, TYPE_BUNDLE) ;
    type[typelen] = 0 ;

    char cmd[typelen + 6 + 1] ;

    auto_strings(cmd,"-t",type," -b") ;
    db = backup_cmd_switcher(VERBOSITY,cmd,&info) ;
    if (db < 0) log_dieu(LOG_EXIT_SYS,"find realpath of symlink for db of tree: ",info.treename.s) ;

    typelen = uint_fmt(type, TYPE_CLASSIC) ;
    type[typelen] = 0 ;

    auto_strings(cmd,"-t",type," -b") ;
    svc = backup_cmd_switcher(VERBOSITY,cmd,&info) ;
    if (svc < 0) log_dieu(LOG_EXIT_SYS,"find realpath of symlink for svc of tree: ",info.treename.s) ;

    if (!bprintf(buffer_1,"%s%s%s%s%s%s%s%s", "svc->",!svc ? log_color->valid : log_color->warning , !svc ? "source" : "backup",log_color->off, " db->", !db ? log_color->valid : log_color->warning, !db ? "source" : "backup", log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    ssexec_free(&info) ;
}

static void info_display_contents(char const *field, char const *treename)
{

    size_t padding = 1, treenamelen = strlen(treename) ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    char tmp[src.len + treenamelen + SS_SVDIRS_LEN + 1] ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    auto_strings(tmp, src.s, treename) ;

    if (!graph_build_service_bytree(&graph, tmp, 2))
        log_dieu(LOG_EXIT_SYS,"build the graph dependencies") ;

    if (!graph_matrix_sort_tosa(&sa, &graph))
        log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

    if (!sa.len) goto empty ;

    if (GRAPH)
    {
        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, 0, treename, &info_graph_display_service, 0, REVERSE, &d, padding, STYLE))
            log_dieu(LOG_EXIT_SYS,"display the graph dependencies") ;

        goto freed ;
    }
    else
    {
        if (REVERSE)
            if (!sastr_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }
    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","/"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
        else
        {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        stralloc_free(&sa) ;
}

static void info_display_groups(char const *field, char const *treename)
{
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (!resolve_read(wres, src.s, treename))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", src.s, "/.resolve/", treename) ;

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

static void info_display_all(char const *treename,int *what)
{

    unsigned int i = 0 ;
    for (; what[i] >= 0 ; i++)
    {
        unsigned int idx = what[i] ;
        (*opts_tree_table[idx].func)(fields[opts_tree_table[idx].id],treename) ;
    }

}

static void info_sort_enabled_notenabled(stralloc *satree,stralloc *enabled,stralloc *not_enabled)
{
    size_t pos = 0 ;

    FOREACH_SASTR(satree,pos) {

        char *ename = satree->s + pos ;

        if (sastr_cmp(enabled,ename) == -1)
            if (!sastr_add_string(not_enabled,ename))
                log_die_nomem("stralloc") ;
    }

    if (not_enabled->len)
        if (!sastr_sort(not_enabled))
            log_dieu(LOG_EXIT_SYS,"sort not enabled tree list") ;

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

int main(int argc, char const *const *argv, char const *const *envp)
{
    unsigned int legacy = 1 ;

    size_t pos, newlen, livelen ;
    int what[MAXOPTS] = { 0 } ;

    log_color = &log_color_disable ;

    char const *treename = 0 ;

    for (int i = 0 ; i < MAXOPTS ; i++)
        what[i] = -1 ;


    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "Name",
        "Current",
        "Enabled",
        "Initialized",
        "Allowed",
        "Symlinks",
        "Groups",
        "Depends",
        "Required by",
        "Contents" } ;


    stralloc satree = STRALLOC_ZERO ;

    PROG = "66-intree" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hzv:no:grd:l:", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :  info_help(); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
                case 'z' :  log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
                case 'n' :  NOFIELD = 0 ; break ;
                case 'o' :  legacy = 0 ; info_parse_options(l.arg,what) ; break ;
                case 'g' :  GRAPH = 1 ; break ;
                case 'r' :  REVERSE = 1 ; break ;
                case 'd' :  if (!uint0_scan(l.arg, &MAXDEPTH)) log_usage(USAGE) ; break ;
                case 'l' :  if (!stralloc_cats(&live,l.arg)) log_usage(USAGE) ;
                            if (!stralloc_0(&live)) log_usage(USAGE) ;
                            break ;
                default :   log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argv[0]) treename = argv[0] ;

    if (legacy)
    {
        unsigned int i = 0 ;
        for (; i < MAXOPTS - 1 ; i++)
            what[i] = i ;

        what[i] = -1 ;
    }

    OWNER = getuid() ;
    size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
    OWNERSTR[ownerlen] = 0 ;

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    setlocale(LC_ALL, "");

    if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
        STYLE = &graph_utf8;
    }

    if (!set_ownersysdir(&base,OWNER)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
    if (!stralloc_copy(&src,&base) ||
    !stralloc_cats(&src,SS_SYSTEM) ||
    !stralloc_0(&src)) log_die_nomem("stralloc") ;
    src.len-- ;

    if (!scan_mode(src.s,S_IFDIR))
    {
        log_info("no tree exist yet") ;
        goto freed ;
    }

    if (!stralloc_cats(&src,"/")) log_die_nomem("stralloc") ;

    newlen = src.len ;
    livelen = live.len ;

    if (treename)
    {

        if (!auto_stra(&src,treename)) log_die_nomem("stralloc") ;
        if (!scan_mode(src.s,S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"find tree: ", src.s) ;
        src.len = newlen ;
        if (!stralloc_0(&src)) log_die_nomem("stralloc") ;
        src.len-- ;
        info_display_all(treename,what) ;
    }
    else
    {
        char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;
        if (!stralloc_0(&src)) log_die_nomem("stralloc") ;
        if (!sastr_dir_get(&satree, src.s,exclude, S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get list of tree at: ",src.s) ;


        if (satree.len) {

            stralloc enabled = STRALLOC_ZERO ;
            stralloc not_enabled = STRALLOC_ZERO ;

            if (!file_readputsa(&enabled,src.s,"state"))
                log_dieusys(LOG_EXIT_SYS,"read contents of file: ",src.s,"/state") ;

            /** May not have enabled tree yet */
            if (enabled.len) {

                if (!sastr_split_string_in_nline(&enabled))
                    log_dieu(LOG_EXIT_SYS,"rebuild enabled tree list") ;
            }

            info_sort_enabled_notenabled(&satree,&enabled,&not_enabled) ;

            {
                pos = 0 ;
                FOREACH_SASTR(&not_enabled,pos) {

                    src.len = newlen ;
                    live.len = livelen ;
                    char *name = not_enabled.s + pos ;
                    info_display_all(name,what) ;
                    if (buffer_puts(buffer_1,"\n") == -1)
                        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
                }
            }

            {
                pos = 0 ;
                FOREACH_SASTR(&enabled,pos) {

                    src.len = newlen ;
                    live.len = livelen ;
                    char *name = enabled.s + pos ;
                    info_display_all(name,what) ;
                    if (buffer_puts(buffer_1,"\n") == -1)
                        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
                }
            }

            stralloc_free(&enabled) ;
            stralloc_free(&not_enabled) ;
        }
        else
        {
            log_info("no tree exist yet") ;
            goto freed ;
        }
    }

    if (buffer_flush(buffer_1) == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    freed:
    stralloc_free(&base) ;
    stralloc_free(&live) ;
    stralloc_free(&src) ;
    stralloc_free(&satree) ;


    return 0 ;
}
