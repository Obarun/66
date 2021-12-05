/*
 * 66-inservice.c
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
#include <unistd.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/graph.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>

#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/environ.h>
#include <66/state.h>
#include <66/service.h>

#include <s6/supervise.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
static char const *const *ENVP ;
static unsigned int nlog = 20 ;
static stralloc src = STRALLOC_ZERO ;
static stralloc home = STRALLOC_ZERO ;// /var/lib/66/system or ${HOME}/system

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_string(char const *str) ;
static void info_display_name(char const *field, resolve_service_t *res) ;
static void info_display_intree(char const *field, resolve_service_t *res) ;
static void info_display_status(char const *field, resolve_service_t *res) ;
static void info_display_type(char const *field, resolve_service_t *res) ;
static void info_display_description(char const *field, resolve_service_t *res) ;
static void info_display_version(char const *field, resolve_service_t *res) ;
static void info_display_source(char const *field, resolve_service_t *res) ;
static void info_display_live(char const *field, resolve_service_t *res) ;
static void info_display_deps(char const *field, resolve_service_t *res) ;
static void info_display_requiredby(char const *field, resolve_service_t *res) ;
static void info_display_optsdeps(char const *field, resolve_service_t *res) ;
static void info_display_extdeps(char const *field, resolve_service_t *res) ;
static void info_display_start(char const *field, resolve_service_t *res) ;
static void info_display_stop(char const *field, resolve_service_t *res) ;
static void info_display_envat(char const *field, resolve_service_t *res) ;
static void info_display_envfile(char const *field, resolve_service_t *res) ;
static void info_display_logname(char const *field, resolve_service_t *res) ;
static void info_display_logdst(char const *field, resolve_service_t *res) ;
static void info_display_logfile(char const *field, resolve_service_t *res) ;

ss_resolve_graph_style *STYLE = &graph_default ;




#include <stdlib.h> //a garder

#include <stdio.h>// a effacer








typedef int ss_info_graph_func(char const *name, char const *obj) ;
typedef ss_info_graph_func *ss_info_graph_func_t_ref ;

int ss_info_graph_display_service(char const *name, char const *obj)
{
    stralloc tree = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    int r = service_resolve_svtree(&tree, name, obj), err = 0 ;

    if (r != 2) {
        if (r == 1)
            log_warnu("find: ", name, " at tree: ", !obj ? tree.s : obj) ;
        if (r > 2)
            log_1_warn(name, " is set on different tree -- please use -t options") ;

        goto freed ;
    }

    if (!resolve_check(tree.s, name))
        goto freed ;

    if (!resolve_read(wres, tree.s, name))
        goto freed ;

    char str_pid[UINT_FMT] ;
    uint8_t pid_color = 0 ;
    char *ppid ;
    ss_state_t sta = STATE_ZERO ;
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;

    if (res.type == TYPE_CLASSIC || res.type == TYPE_LONGRUN) {

        s6_svstatus_read(res.sa.s + res.runat ,&status) ;
        pid_color = !status.pid ? 1 : 2 ;
        str_pid[uint_fmt(str_pid, status.pid)] = 0 ;
        ppid = &str_pid[0] ;

    } else {

        char *ste = res.sa.s + res.state ;
        char *name = res.sa.s + res.name ;
        if (!state_check(ste,name)) {

            ppid = "unitialized" ;
            goto dis ;
        }

        if (!state_read(&sta,ste,name)) {

            log_warnu("read state of: ",name) ;
            goto freed ;
        }
        if (sta.init) {

            ppid = "unitialized" ;
            goto dis ;

        } else if (!sta.state) {

            ppid = "down" ;
            pid_color = 1 ;

        } else if (sta.state) {

            ppid = "up" ;
            pid_color = 2 ;
        }
    }

    dis:

    if (!bprintf(buffer_1,"(%s%s%s,%s%s%s,%s) %s", \

                pid_color > 1 ? log_color->valid : pid_color ? log_color->error : log_color->warning, \
                ppid, \
                log_color->off, \

                res.disen ? log_color->off : log_color->error, \
                res.disen ? "Enabled" : "Disabled", \
                log_color->off, \

                get_key_by_enum(ENUM_TYPE,res.type), \

                name))
                    goto freed ;

    err = 1 ;

    freed:
        resolve_free(wres) ;
        stralloc_free(&tree) ;

    return err ;

}

int ss_info_graph_display(char const *name, char const *obj, ss_info_graph_func *func, depth_t *depth, int last, int padding, ss_resolve_graph_style *style)
{
    log_flow() ;

    int level = 1 ;

    const char *tip = "" ;

    tip = last ? style->last : style->tip ;

    while(depth->prev)
        depth = depth->prev ;

    while(depth->next)
    {
        if (!bprintf(buffer_1,"%*s%-*s",style->indent * (depth->level - level) + (level == 1 ? padding : 0), "", style->indent, style->limb))
            return 0 ;

        level = depth->level + 1 ;
        depth = depth->next ;
    }

    if (!bprintf(buffer_1,"%*s%*s%s", \
                level == 1 ? padding : 0,"", \
                style->indent * (depth->level - level), "", \
                tip)) return 0 ;

    int r = (*func)(name, obj) ;
    if (!r) return 0 ;
    if (buffer_putsflush(buffer_1,"\n") < 0)
        return 0 ;

    return 1 ;
}












/** make a list of edge ordered by start order
 * Return the number of edge on success
 * Return -1 on fail */
int ss_info_walk_edge(stralloc *sa, graph_t *g, char const *name, uint8_t requiredby)
{

    size_t pos = 0 ;
    int count = -1 ;

    graph_t gc = GRAPH_ZERO ;

    sa->len = 0 ;
    count = graph_matrix_get_edge_g(sa, g, name, requiredby) ;

    size_t len = sa->len ;
    char vertex[len + 1] ;

    if (count < 0) {
        count = 0 ;
        goto freed ;
    }

    sastr_to_char(vertex,sa) ;
    count = -1 ;

    for (; pos < len ; pos += strlen(vertex + pos) + 1) {

        sa->len = 0 ;

        char *ename = vertex + pos ;

        if (!graph_vertex_add(&gc, ename)) {

            log_warnu("add vertex: ",ename) ;
            goto freed ;
        }

        unsigned int c = graph_matrix_get_edge_g(sa, g, ename, requiredby) ;

        if (c == -1)
            goto freed ;

        {
            size_t bpos = 0 ;

            FOREACH_SASTR(sa, bpos) {

                char *edge = sa->s + bpos ;

                if (!graph_vertex_add_with_edge(&gc, ename, edge)) {

                    log_warnu("add edge: ",ename," to vertex: ", edge) ;
                    goto freed ;
                }

                if (!graph_vertex_add(&gc, edge)) {

                    log_warnu("add vertex: ",edge) ;
                    goto freed ;
                }
            }
        }
    }

    if (!graph_matrix_build(&gc)) {

        log_warnu("build the matrix") ;
        goto freed ;
    }

    if (!graph_matrix_analyze_cycle(&gc)) {

        log_warnu("found cycle") ;
        goto freed ;
    }

    if (!graph_matrix_sort(&gc)) {

        log_warnu("sort the matrix") ;
        goto freed ;
    }


    sa->len = pos = 0 ;

    for(; pos < gc.sort_count ; pos++) {

        char *name = gc.data.s + genalloc_s(graph_hash_t,&gc.hash)[gc.sort[pos]].vertex ;

        if (!sastr_add_string(sa, name)) {

            log_warnu("add string") ;
            goto freed ;
        }

    }

    count = gc.sort_count ;

    freed:
        graph_free_all(&gc) ;
        return count ;
}

int ss_info_walk(graph_t *g, char const *name, char const *obj, ss_info_graph_func *func, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, ss_resolve_graph_style *style)
{
    log_flow() ;

    int e = 0, idx = 0 ;
    size_t pos = 0, len ;

    if ((unsigned int) depth->level > MAXDEPTH)
        return 1 ;

    stralloc sa = STRALLOC_ZERO ;

    int count = ss_info_walk_edge(&sa, g, name, requiredby) ;

    len = sa.len ;
    char vertex[len + 1] ;

    if (count == -1) goto err ;

    if (!sa.len)
        goto freed ;

    if (reverse)
        if (!sastr_reverse(&sa))
            goto err ;

    sastr_to_char(vertex, &sa) ;

    for (; pos < len ; pos += strlen(vertex + pos) + 1, idx++ ) {

        sa.len = 0 ;
        int last =  idx + 1 < count  ? 0 : 1 ;
        char *name = vertex + pos ;

        if (!ss_info_graph_display(name, obj, func, depth, last, padding, style))
            goto err ;

        if (ss_info_walk_edge(&sa, g, name, requiredby) == -1)
            goto err ;

        if (sa.len)
        {
            depth_t d =
            {
                depth,
                NULL,
                depth->level + 1
            } ;
            depth->next = &d;

            if(last)
            {
                if(depth->prev)
                {
                    depth->prev->next = &d;
                    d.prev = depth->prev;
                    depth = &d;

                }
                else
                    d.prev = NULL;
            }
            if (!ss_info_walk(g, name, obj, func, requiredby, reverse, &d, padding, style))
                goto err ;
            depth->next = NULL ;
        }
    }

    freed:
        e = 1 ;

    err:
        stralloc_free(&sa) ;

    return e ;

}

depth_t ss_info_graph_init(void)
{
    log_flow() ;

    depth_t d = {
        NULL,
        NULL,
        1
    } ;

    return d ;
}













info_opts_map_t const opts_sv_table[] =
{
    { .str = "name", .svfunc = &info_display_name, .id = 0 },
    { .str = "version", .svfunc = &info_display_version, .id = 1 },
    { .str = "intree", .svfunc = &info_display_intree, .id = 2 },
    { .str = "status", .svfunc = &info_display_status, .id = 3 },
    { .str = "type", .svfunc = &info_display_type, .id = 4 },
    { .str = "description", .svfunc = &info_display_description, .id = 5 },
    { .str = "source", .svfunc = &info_display_source, .id = 6 },
    { .str = "live", .svfunc = &info_display_live, .id = 7 },
    { .str = "depends", .svfunc = &info_display_deps, .id = 8 },
    { .str = "requiredby", .svfunc = &info_display_requiredby, .id = 9 },
    { .str = "extdepends", .svfunc = &info_display_extdeps, .id = 10 },
    { .str = "optsdepends", .svfunc = &info_display_optsdeps, .id = 11 },
    { .str = "start", .svfunc = &info_display_start, .id = 12 },
    { .str = "stop", .svfunc = &info_display_stop, .id = 13 },
    { .str = "envat", .svfunc = &info_display_envat, .id = 14 },
    { .str = "envfile", .svfunc = &info_display_envfile, .id = 15 },
    { .str = "logname", .svfunc = &info_display_logname, .id = 16 },
    { .str = "logdst", .svfunc = &info_display_logdst, .id = 17 },
    { .str = "logfile", .svfunc = &info_display_logfile, .id = 18 },
    { .str = 0, .svfunc = 0, .id = -1 }
} ;

#define MAXOPTS 20
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

#define USAGE "66-inservice [ -h ] [ -z ] [ -v verbosity ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -t tree ] [ -p nline ] service"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -n: do not display the field name\n"
"   -o: comma separated list of field to display\n"
"   -g: displays the contents field as graph\n"
"   -d: limit the depth of the contents field recursion\n"
"   -r: reverse the contents field\n"
"   -t: only search service at the specified tree\n"
"   -p: print n last lines of the log file\n"
"\n"
"valid fields for -o options are:\n"
"\n"
"   name: displays the name\n"
"   version: displays the version of the service\n"
"   intree: displays the service's tree name\n"
"   status: displays the status\n"
"   type: displays the service type\n"
"   description: displays the description\n"
"   source: displays the source of the service's frontend file\n"
"   live: displays the service's live directory\n"
"   depends: displays the service's dependencies\n"
"   requiredby: displays the service(s) which depends on service\n"
"   extdepends: displays the service's external dependencies\n"
"   optsdepends: displays the service's optional dependencies\n"
"   start: displays the service's start script\n"
"   stop: displays the service's stop script\n"
"   envat: displays the source of the environment file\n"
"   envfile: displays the contents of the environment file\n"
"   logname: displays the logger's name\n"
"   logdst: displays the logger's destination\n"
"   logfile: displays the contents of the log file\n"
;

    log_info(USAGE,"\n",help) ;
}

char *print_nlog(char *str, int n)
{
    int r = 0 ;
    int delim ='\n' ;
    size_t slen = strlen(str) ;

    if (n <= 0) return NULL;

    size_t ndelim = 0;
    char *target_pos = NULL;

    r = get_rlen_until(str,delim,slen) ;

    target_pos = str + r ;

    if (target_pos == NULL) return NULL;

    while (ndelim <= n)
    {
        while (str < target_pos && *target_pos != delim)
            --target_pos;

        if (*target_pos ==  delim)
            --target_pos, ++ndelim;
        else break;
    }

    if (str < target_pos)
        target_pos += 2;

    return target_pos ;
}

static void info_display_string(char const *str)
{
    if (!bprintf(buffer_1,"%s",str))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_name(char const *field, resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->name) ;
}

static void info_display_version(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    /** tempory check here, it not mandatory for the moment*/
    if (res->version > 0)
    {
        info_display_string(res->sa.s + res->version) ;
    }
    else
    {
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_intree(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->treename) ;
}

static void info_get_status(resolve_service_t *res)
{
    int r ;
    int wstat ;
    pid_t pid ;
    ss_state_t sta = STATE_ZERO ;
    int warn_color = 0 ;
    if (res->type == TYPE_CLASSIC || res->type == TYPE_LONGRUN)
    {
        r = s6_svc_ok(res->sa.s + res->runat) ;
        if (r != 1)
        {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            return ;
        }
        char const *newargv[3] ;
        unsigned int m = 0 ;

        newargv[m++] = SS_BINPREFIX "s6-svstat" ;
        newargv[m++] = res->sa.s + res->runat ;
        newargv[m++] = 0 ;

        pid = child_spawn0(newargv[0],newargv,ENVP) ;
        if (waitpid_nointr(pid,&wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS,"wait for ",newargv[0]) ;

        if (wstat)
            log_dieu(LOG_EXIT_SYS,"status for service: ",res->sa.s + res->name) ;
    }
    else
    {
        char *ste = res->sa.s + res->state ;
        char *name = res->sa.s + res->name ;
        char *status = 0 ;
        if (!state_check(ste,name))
        {
            status = "unitialized" ;
            goto dis ;
        }

        if (!state_read(&sta,ste,name))
            log_dieusys(LOG_EXIT_SYS,"read state of: ",name) ;

        if (sta.init) {
            status = "unitialized" ;
        }
        else if (!sta.state)
        {
            status = "down" ;
            warn_color = 1 ;
        }
        else if (sta.state)
        {
            status = "up" ;
            warn_color = 2 ;
        }
        dis:
        if (!bprintf(buffer_1,"%s%s%s\n",warn_color > 1 ? log_color->valid : warn_color ? log_color->error : log_color->warning,status,log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_status(char const *field,resolve_service_t *res)
{

    if (NOFIELD) info_display_field_name(field) ;

    if (!bprintf(buffer_1,"%s%s%s%s",res->disen ? log_color->valid : log_color->error,res->disen ? "enabled" : "disabled",log_color->off,", "))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    info_get_status(res) ;

}

static void info_display_type(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(get_key_by_enum(ENUM_TYPE,res->type)) ;
}

static void info_display_description(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->description) ;
}

static void info_display_source(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->src) ;
}

static void info_display_live(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->runat) ;
}


/*
int graph_service_compute_deps(stralloc *deps, char const *str)
{
    stralloc tmp = STRALLOC_ZERO ;
    size_t pos = 0 ;

    if (!sastr_clean_string(&tmp,str))
        log_warnu_return(LOG_EXIT_ZERO,"rebuild dependencies list") ;

    FOREACH_SASTR(&tmp,pos) {

            if (!sastr_add_string(deps, tmp.s + pos))
                return 0 ;
    }

    stralloc_free(&tmp) ;

    return 1 ;
}
*/

int graph_service_add_deps(graph_t *g, char const *service, char const *sdeps)
{
    stralloc deps = STRALLOC_ZERO ;
    uint8_t e = 0 ;
    if (!sastr_clean_string(&deps,sdeps)) {
        log_warnu("rebuild dependencies list") ;
        goto freed ;
    }

    if (!graph_vertex_add_with_nedge(g, service, &deps)) {
        log_warnu("add edges at vertex: ", service) ;
        goto freed ;
    }

    e = 1 ;
    freed:
    stralloc_free(&deps) ;

    return e ;
}


void ss_graph_matrix_add_classic(graph_t *g, genalloc *gares)
{
    size_t pos = 0, bpos = 0, ccount = 0 ;
    size_t cl[SS_MAX_SERVICE] ;

    for (; pos < genalloc_len(resolve_service_t, gares) ; pos++) {

        resolve_service_t_ref res = &genalloc_s(resolve_service_t, gares)[pos] ;

        if (res->type == TYPE_CLASSIC) {

            cl[ccount++] = pos ;

        }
    }

    if (ccount) {

        for (pos = 0 ; pos < ccount ; pos++)  {

            char *str = genalloc_s(resolve_service_t, gares)[cl[pos]].sa.s ;
            char *sv = str + genalloc_s(resolve_service_t, gares)[cl[pos]].name ;
printf("sv::%s\n",sv) ;
            graph_array_reverse(g->sort, g->sort_count) ;

            for (bpos = 0 ; bpos < g->sort_count ; bpos++) {

                char *service = g->data.s + genalloc_s(graph_hash_t,&g->hash)[g->sort[bpos]].vertex ;

                int idx = resolve_search(gares, service, SERVICE_STRUCT) ;
                if (genalloc_s(resolve_service_t, gares)[idx].type == TYPE_CLASSIC ||
                    !strcmp(service, sv))
                        continue ;

                if (!graph_edge_add_g(g, service, sv))
                    log_die(LOG_EXIT_SYS,"add edge: ", sv, " at vertex: ", service) ;

                graph_free_matrix(g) ;
                graph_free_sort(g) ;

                if (!graph_matrix_build(g)) {
                    graph_free_all(g) ;
                    log_dieu(LOG_EXIT_SYS,"build the graph") ;
                }

                if (!graph_matrix_analyze_cycle(g))
                    log_die(LOG_EXIT_SYS,"found cycle") ;
            }

            graph_array_reverse(g->sort, g->sort_count) ;
        }
    }

}

/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both
 * @Return 0 on fail
 *
 * This function append the logger to @gares is case of classic service. */
int ss_tree_get_sv_resolve(genalloc *gares, char const *dir, uint8_t what)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    resolve_service_t reslog = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wreslog = resolve_set_struct(SERVICE_STRUCT, &reslog) ;

    int e = 0 ;
    size_t dirlen = strlen(dir), pos = 0 ;

    char solve[dirlen + SS_RESOLVE_LEN + 1] ;

    auto_strings(solve, dir, SS_RESOLVE) ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;
    if (!sastr_dir_get(&sa,solve,exclude,S_IFREG))
        goto err ;

    FOREACH_SASTR(&sa, pos) {

        char *name = sa.s + pos ;

        if (!resolve_check(dir,name))
            goto err ;

        if (!resolve_read(wres,dir,name))
            goto err ;

        if (resolve_search(gares,name, SERVICE_STRUCT) == -1) {

            if ((!what || what == 2) && (res.type == TYPE_CLASSIC)) {

                if (res.logger) {

                    if (!resolve_read(wreslog, dir, res.sa.s + res.logger))
                        goto err ;

                    if (resolve_search(gares,res.sa.s + res.logger, SERVICE_STRUCT) == -1) {

                        if (!resolve_append(gares,wreslog))
                            goto err ;
                    }
                }

                if (!resolve_append(gares,wres))
                    goto err ;

                continue ;
            }

            if (what) {

                if (!resolve_append(gares,wres))
                    goto err ;
            }
        }
    }

    e = 1 ;
    err:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        resolve_free(wreslog) ;
        return e ;
}

/** @tree: absolute path of the tree*/
static void ss_graph_matrix_build_bytree(graph_t *g, char const *tree, uint8_t what)
{
    stralloc services = STRALLOC_ZERO ;
    stralloc deps = STRALLOC_ZERO ;
    genalloc gares = GENALLOC_ZERO ;

    size_t treelen = strlen(tree), pos = 0 ;
    char src[treelen + SS_SVDIRS_LEN + 1] ;

    auto_strings(src, tree, SS_SVDIRS) ;

    if (!ss_tree_get_sv_resolve(&gares, src, what))
        log_dieu(LOG_EXIT_SYS,"get resolve files of tree: ", tree) ;

    if (genalloc_len(resolve_service_t, &gares) >= SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services to handle") ;

    pos = 0 ;

    for (; pos < genalloc_len(resolve_service_t, &gares) ; pos++) {

        resolve_service_t_ref res = &genalloc_s(resolve_service_t, &gares)[pos] ;

        char *str = res->sa.s ;

        char *service = str + res->name ;

        if (!graph_vertex_add(g, service))
            log_dieu(LOG_EXIT_SYS,"add vertex: ", service) ;

        deps.len = 0 ;

        if (res->ndeps > 0) {

            if (res->type == TYPE_MODULE || res->type == TYPE_BUNDLE) {

                uint32_t tdeps = res->type == TYPE_MODULE ? what > 1 ? res->contents : res->deps : res->deps ;

                if (!graph_service_add_deps(g, service, str + tdeps))
                    log_dieu(LOG_EXIT_ZERO,"add dependencies of service: ",service) ;

            } else {

                if (!graph_service_add_deps(g, service,str + res->deps))
                    log_dieu(LOG_EXIT_ZERO,"add dependencies of service: ",service) ;

            }
        }
    }


    if (!graph_matrix_build(g))
        log_dieu(LOG_EXIT_SYS,"build the graph") ;

    if (!graph_matrix_analyze_cycle(g))
        log_die(LOG_EXIT_SYS,"found cycle") ;

    if (!graph_matrix_sort(g))
        log_dieu(LOG_EXIT_SYS,"sort the graph") ;

    stralloc_free(&services) ;
    stralloc_free(&deps) ;
}



static void info_display_requiredby(char const *field, resolve_service_t *res)
{
    size_t padding = 1 ;
    int r ;
    graph_t graph = GRAPH_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    /**
     *
     *
     * ATTENTION A LA SORTIE D ERREUR
     *
     * */

    ss_graph_matrix_build_bytree(&graph, res->sa.s + res->tree, 2) ;


    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_requiredby(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get requiredby for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = ss_info_graph_init() ;

        if (!ss_info_walk(&graph, res->sa.s + res->name, res->sa.s + res->treename, &ss_info_graph_display_service, 1, REVERSE, &d, padding, STYLE))
            log_dieu(LOG_EXIT_SYS,"display the requiredby graphic") ;

        return ;

    } else {

        deps.len = 0 ;
        r = ss_info_walk_edge(&deps,&graph, res->sa.s + res->name, 1) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "get requiredby list") ;

        if (!r)
            goto empty ;

        if (REVERSE)
            if (!sastr_reverse(&deps))
                log_dieu(LOG_EXIT_SYS,"reverse dependencies list") ;

        info_display_list(field,&deps) ;

        return ;
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
}

static void info_display_deps(char const *field, resolve_service_t *res)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;

    stralloc deps = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    ss_graph_matrix_build_bytree(&graph, res->sa.s + res->tree, 2) ;

    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_edge(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get requiredby for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH)
    {
        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = ss_info_graph_init() ;

        if (!ss_info_walk(&graph, res->sa.s + res->name, res->sa.s + res->treename, &ss_info_graph_display_service, 0, REVERSE, &d, padding, STYLE))
            log_dieu(LOG_EXIT_SYS,"display the dependencies graphic") ;

        goto freed ;
    }
    else
    {
        deps.len = 0 ;
        r = ss_info_walk_edge(&deps,&graph, res->sa.s + res->name, 0) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "get requiredby list") ;

        if (!r)
            goto empty ;

        if (REVERSE)
            if (!sastr_reverse(&deps))
                log_dieu(LOG_EXIT_SYS,"reverse dependencies list") ;
        info_display_list(field,&deps) ;

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
        stralloc_free(&deps) ;
}

static void info_display_with_source_tree(stralloc *list,resolve_service_t *res)
{
    size_t pos = 0, lpos = 0, newlen = 0 ;
    stralloc svlist = STRALLOC_ZERO ;
    stralloc ntree = STRALLOC_ZERO ;
    stralloc src = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;
    char *treename = 0 ;

    if (!auto_stra(&src,home.s)) log_die_nomem("stralloc") ;
    newlen = src.len ;


    if (!sastr_dir_get(&ntree,home.s,exclude,S_IFDIR))
        log_dieu(LOG_EXIT_SYS,"get list of trees of: ",home.s) ;

    for (pos = 0 ; pos < ntree.len ; pos += strlen(ntree.s + pos) + 1)
    {
        svlist.len = 0 ;
        src.len = newlen ;
        treename = ntree.s + pos ;

        if (!auto_stra(&src,treename,SS_SVDIRS,SS_RESOLVE))
            log_die_nomem("stralloc") ;

        exclude[0] = SS_MASTER + 1 ;
        exclude[1] = 0 ;
        if (!sastr_dir_get(&svlist,src.s,exclude,S_IFREG))
            log_dieu(LOG_EXIT_SYS,"get contents of tree: ",src.s) ;

        for (lpos = 0 ; lpos < list->len ; lpos += strlen(list->s + lpos) + 1)
        {
            char *name = list->s + lpos ;
            if (sastr_cmp(&svlist,name) >= 0)
            {
                if (!stralloc_cats(&tmp,name) ||
                !stralloc_cats(&tmp,":") ||
                !stralloc_catb(&tmp,treename,strlen(treename) +1))
                    log_die_nomem("stralloc") ;
            }
        }
    }

    list->len = 0 ;
    for (pos = 0 ; pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
        if (!stralloc_catb(list,tmp.s + pos,strlen(tmp.s + pos) + 1))
            log_die_nomem("stralloc") ;

    stralloc_free (&svlist) ;
    stralloc_free (&ntree) ;
    stralloc_free (&src) ;
    stralloc_free (&tmp) ;
}

static void info_display_optsdeps(char const *field, resolve_service_t *res)
{
    stralloc salist = STRALLOC_ZERO ;

    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    if (!res->noptsdeps) goto empty ;

    if (!sastr_clean_string(&salist,res->sa.s + res->optsdeps))
        log_dieu(LOG_EXIT_SYS,"build dependencies list") ;

    info_display_with_source_tree(&salist,res) ;

    if (REVERSE)
        if (!sastr_reverse(&salist))
                log_dieu(LOG_EXIT_SYS,"reverse dependencies list") ;

    info_display_list(field,&salist) ;

    goto freed ;

    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    freed:
        stralloc_free(&salist) ;
}

static void info_display_extdeps(char const *field, resolve_service_t *res)
{
    stralloc salist = STRALLOC_ZERO ;

    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    if (!res->nextdeps) goto empty ;

    if (!sastr_clean_string(&salist,res->sa.s + res->extdeps))
        log_dieu(LOG_EXIT_SYS,"build dependencies list") ;

    info_display_with_source_tree(&salist,res) ;

    if (REVERSE)
        if (!sastr_reverse(&salist))
                log_dieu(LOG_EXIT_SYS,"reverse dependencies list") ;

    info_display_list(field,&salist) ;

    goto freed ;

    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    freed:
        stralloc_free(&salist) ;
}

static void info_display_start(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;
    if (res->exec_run)
    {
        info_display_nline(field,res->sa.s + res->exec_run) ;
    }
    else
    {
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_stop(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;
    if (res->exec_finish)
    {
        info_display_nline(field,res->sa.s + res->exec_finish) ;
    }
    else
    {
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }

}

static void info_display_envat(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    stralloc salink = STRALLOC_ZERO ;

    if (!res->srconf) goto empty ;
    {
        stralloc salink = STRALLOC_ZERO ;
        char *src = res->sa.s + res->srconf ;

        size_t srclen = strlen(src) ;
        char sym[srclen + SS_SYM_VERSION_LEN + 1] ;

        auto_strings(sym,src,SS_SYM_VERSION) ;

        if (sareadlink(&salink, sym) == -1)
            log_dieusys(LOG_EXIT_SYS,"read link of: ",sym) ;

        if (!stralloc_0(&salink))
            log_die_nomem("stralloc") ;

        info_display_string(salink.s) ;

        stralloc_free(&salink) ;

        goto freed ;
    }
    empty:

    if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    freed:
        stralloc_free(&salink) ;
}

static void info_display_envfile(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    stralloc salink = STRALLOC_ZERO ;
    stralloc list = STRALLOC_ZERO ;
    char const *exclude[1] = { 0 } ;

    if (res->srconf)
    {
        char *src = res->sa.s + res->srconf ;
        size_t srclen = strlen(src), newlen ;
        char sym[srclen + SS_SYM_VERSION_LEN + 1] ;

        auto_strings(sym,src,SS_SYM_VERSION) ;

        if (sareadlink(&salink, sym) == -1)
            log_dieusys(LOG_EXIT_SYS,"read link of: ",sym) ;

        if (!stralloc_0(&salink))
            log_dieusys(LOG_EXIT_SYS,"stralloc") ;

        newlen = salink.len - 1 ;

        if (!sastr_dir_get(&list,salink.s,exclude,S_IFREG))
            log_dieusys(LOG_EXIT_SYS,"get list of environment file from: ",src) ;

        if (!sastr_sort(&list))
            log_dieu(LOG_EXIT_SYS,"sort environment file name") ;

        FOREACH_SASTR(&list,pos) {

            ssize_t upstream = 0 ;
            sa.len = 0 ;
            salink.len = newlen ;
            if (!stralloc_cats(&salink,"/") ||
            !stralloc_cats(&salink,list.s + pos) ||
            !stralloc_0(&salink)) log_die_nomem("stralloc") ;

            if (!file_readputsa_g(&sa,salink.s))
                log_dieusys(LOG_EXIT_SYS,"read environment file") ;

            /** Remove warning message */
            if (list.s[pos] == '.') {

                char t[sa.len + 1] ;

                upstream = str_contain(sa.s,"[ENDWARN]") ;

                if (upstream == -1)
                    log_die(LOG_EXIT_SYS,"invalid upstream configuration file! Do you have modified it? Tries to enable the service again.") ;

                auto_strings(t,sa.s + upstream) ;

                sa.len = 0 ;

                if (!auto_stra(&sa,t))
                    log_die_nomem("stralloc") ;
            }

            if (NOFIELD) {

                char *m = "environment variables from: " ;
                size_t mlen = strlen(m) ;
                char msg[mlen + salink.len + 2] ;
                auto_strings(msg,m,salink.s,"\n") ;
                if (!stralloc_inserts(&sa,0,msg) ||
                !stralloc_0(&sa))
                    log_die_nomem("stralloc") ;

            }

            if (pos)
            {
                if (NOFIELD) {
                    size_t padding = info_length_from_wchar(field) + 1 ;
                    if (!bprintf(buffer_1,"%*s",padding,""))
                        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
                }
                info_display_nline(field,sa.s) ;
            }
            else info_display_nline(field,sa.s) ;

            if (!bprintf(buffer_1,"%s","\n"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
    }
    else
    {
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }

    stralloc_free(&sa) ;
    stralloc_free(&list) ;
    stralloc_free(&salink) ;
}

static void info_display_logname(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (res->type == TYPE_CLASSIC || res->type == TYPE_LONGRUN)
    {
        if (res->logger)
        {
            info_display_string(res->sa.s + res->logger) ;
        }
        else goto empty ;
    }
    else goto empty ;

    return ;
    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_logdst(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (res->type != TYPE_BUNDLE || res->type != TYPE_MODULE)
    {
        if (res->logger || (res->type == TYPE_ONESHOT && res->dstlog))
        {
            info_display_string(res->sa.s + res->dstlog) ;
        }
        else goto empty ;
    }
    else goto empty ;

    return ;
    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_logfile(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    if (res->type != TYPE_BUNDLE || res->type != TYPE_MODULE)
    {
        if (res->logger || (res->type == TYPE_ONESHOT && res->dstlog))
        {
            if (nlog)
            {
                stralloc log = STRALLOC_ZERO ;
                /** the file current may not exist if the service was never started*/
                size_t dstlen = strlen(res->sa.s + res->dstlog) ;
                char scan[dstlen + 9] ;
                memcpy(scan,res->sa.s + res->dstlog,dstlen) ;
                memcpy(scan + dstlen,"/current",8) ;
                scan[dstlen + 8] = 0 ;
                int r = scan_mode(scan,S_IFREG) ;
                if (r < 0) { errno = EEXIST ; log_diesys(LOG_EXIT_SYS,"conflicting format of: ",scan) ; }
                if (!r)
                {
                    if (!bprintf(buffer_1,"%s%s%s\n",log_color->error,"unable to find the log file",log_color->off))
                    goto err ;
                }
                else
                {
                    if (!file_readputsa(&log,res->sa.s + res->dstlog,"current")) log_dieusys(LOG_EXIT_SYS,"read log file of: ",res->sa.s + res->name) ;
                    /* we don't need to freed stralloc
                     * file_readputsa do it if the file is empty*/
                    if (!log.len) goto empty ;
                    log.len-- ;
                    if (!auto_stra(&log,"\n")) log_dieusys(LOG_EXIT_SYS,"append newline") ;
                    if (log.len < 10 && res->type != TYPE_ONESHOT)
                    {
                        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off)) goto err ;
                    }
                    else
                    {
                        if (!bprintf(buffer_1,"\n")) goto err ;
                        if (!bprintf(buffer_1,"%s\n",print_nlog(log.s,nlog))) goto err ;
                    }
                }
                stralloc_free(&log) ;
            }
        }else goto empty ;
    }
    else goto empty ;

    return ;
    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        return ;
    err:
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_all(resolve_service_t *res,int *what)
{

    unsigned int i = 0 ;
    for (; what[i] >= 0 ; i++)
    {
        unsigned int idx = what[i] ;
        (*opts_sv_table[idx].svfunc)(fields[opts_sv_table[idx].id],res) ;
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

    for (;pos < sa.len; pos += strlen(sa.s + pos) + 1)
    {
        char *o = sa.s + pos ;
        t = opts_sv_table ;
        old = nopts ;
        for (; t->str; t++)
        {
            if (obstr_equal(o,t->str))
                what[nopts++] = t->id ;
        }
        if (old == nopts) log_die(LOG_EXIT_SYS,"invalid option: ",o) ;
    }

    stralloc_free(&sa) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
    unsigned int legacy = 1 ;
    int found = 0 ;
    int what[MAXOPTS] = { 0 } ;

    uid_t owner ;
    char ownerstr[UID_FMT] ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    stralloc satree = STRALLOC_ZERO ;

    log_color = &log_color_disable ;

    char const *svname = 0 ;
    char const *tname = 0 ;

    ENVP = envp ;

    for (int i = 0 ; i < MAXOPTS ; i++)
        what[i] = -1 ;


    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "Name",
        "Version" ,
        "In tree",
        "Status",
        "Type",
        "Description",
        "Source",
        "Live",
        "Dependencies",
        "Required by",
        "External dependencies" ,
        "Optional dependencies" ,
        "Start script",
        "Stop script",
        "Environment source",
        "Environment file",
        "Log name",
        "Log destination",
        "Log file" } ;

    PROG = "66-inservice" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, "hzv:cno:grd:t:p:", &l) ;
            if (opt == -1) break ;

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
                case 't' :  tname = l.arg ; break ;
                case 'p' :  if (!uint0_scan(l.arg, &nlog)) log_usage(USAGE) ; break ;
                default :   log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc) log_usage(USAGE) ;
    svname = *argv ;

    if (legacy)
    {
        unsigned int i = 0 ;
        for (; i < MAXOPTS - 1 ; i++)
            what[i] = i ;

        what[i] = -1 ;
    }

    owner = getuid() ;
    size_t ownerlen = uid_fmt(ownerstr,owner) ;
    ownerstr[ownerlen] = 0 ;

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    setlocale(LC_ALL, "");

    if(!strcmp(nl_langinfo(CODESET), "UTF-8")) {
        STYLE = &graph_utf8;
    }
    if (!set_ownersysdir(&home,owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
    if (!auto_stra(&home,SS_SYSTEM,"/")) log_die_nomem("stralloc") ;

    found = service_resolve_svtree(&src,svname,tname) ;

    if (found == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of service: ",svname) ;
    else if (!found) {
        log_info("no tree exist yet") ;
        goto freed ;
    }
    else if (found > 2) {
        log_die(LOG_EXIT_SYS,svname," is set on different tree -- please use -t options") ;
    }
    else if (found == 1) log_die(LOG_EXIT_SYS,"unknown service: ",svname) ;

    if (!resolve_read(wres,src.s,svname))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",svname) ;

    info_display_all(&res,what) ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    freed:
    resolve_free(wres) ;
    stralloc_free(&src) ;
    stralloc_free(&home) ;
    stralloc_free(&satree) ;

    return 0 ;

}
