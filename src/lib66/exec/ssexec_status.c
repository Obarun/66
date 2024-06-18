/*
 * ssexec_status.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>
#include <wchar.h>
#include <unistd.h>
#include <stdlib.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/graph.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <skalibs/cspawn.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/env.h>

#include <66/info.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/environ.h>
#include <66/state.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>

#include <s6/supervise.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
static unsigned int nlog = 20 ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_string(char const *str) ;
static void info_display_name(char const *field, resolve_service_t *res) ;
static void info_display_version(char const *field, resolve_service_t *res) ;
static void info_display_intree(char const *field, resolve_service_t *res) ;
static void info_display_status(char const *field, resolve_service_t *res) ;
static void info_display_type(char const *field, resolve_service_t *res) ;
static void info_display_description(char const *field, resolve_service_t *res) ;
static void info_display_inns(char const *field, resolve_service_t *res) ;
static void info_display_notify(char const *field, resolve_service_t *res) ;
static void info_display_maxdeath(char const *field, resolve_service_t *res) ;
static void info_display_earlier(char const *field, resolve_service_t *res) ;
static void info_display_source(char const *field, resolve_service_t *res) ;
static void info_display_live(char const *field, resolve_service_t *res) ;
static void info_display_deps(char const *field, resolve_service_t *res) ;
static void info_display_requiredby(char const *field, resolve_service_t *res) ;
static void info_display_contents(char const *field, resolve_service_t *res) ;
static void info_display_optsdeps(char const *field, resolve_service_t *res) ;
static void info_display_start(char const *field, resolve_service_t *res) ;
static void info_display_stop(char const *field, resolve_service_t *res) ;
static void info_display_envat(char const *field, resolve_service_t *res) ;
static void info_display_envfile(char const *field, resolve_service_t *res) ;
static void info_display_logname(char const *field, resolve_service_t *res) ;
static void info_display_logdst(char const *field, resolve_service_t *res) ;
static void info_display_logfile(char const *field, resolve_service_t *res) ;

static info_graph_style *S_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;

static info_opts_map_t const opts_sv_table[] =
{
    { .str = "name", .svfunc = &info_display_name, .id = 0 },
    { .str = "version", .svfunc = &info_display_version, .id = 1 },
    { .str = "intree", .svfunc = &info_display_intree, .id = 2 },
    { .str = "status", .svfunc = &info_display_status, .id = 3 },
    { .str = "type", .svfunc = &info_display_type, .id = 4 },
    { .str = "description", .svfunc = &info_display_description, .id = 5 },
    { .str = "partof", .svfunc = &info_display_inns, .id = 6 },
    { .str = "notify", .svfunc = &info_display_notify, .id = 7 },
    { .str = "maxdeath", .svfunc = &info_display_maxdeath, .id = 8 },
    { .str = "earlier", .svfunc = &info_display_earlier, .id = 9 },
    { .str = "source", .svfunc = &info_display_source, .id = 10 },
    { .str = "live", .svfunc = &info_display_live, .id = 11 },
    { .str = "depends", .svfunc = &info_display_deps, .id = 12 },
    { .str = "requiredby", .svfunc = &info_display_requiredby, .id = 13 },
    { .str = "contents", .svfunc = &info_display_contents, .id = 14 },
    { .str = "optsdepends", .svfunc = &info_display_optsdeps, .id = 15 },
    { .str = "start", .svfunc = &info_display_start, .id = 16 },
    { .str = "stop", .svfunc = &info_display_stop, .id = 17 },
    { .str = "envat", .svfunc = &info_display_envat, .id = 18 },
    { .str = "envfile", .svfunc = &info_display_envfile, .id = 19 },
    { .str = "logname", .svfunc = &info_display_logname, .id = 20 },
    { .str = "logdst", .svfunc = &info_display_logdst, .id = 21 },
    { .str = "logfile", .svfunc = &info_display_logfile, .id = 22 },
    { .str = 0, .svfunc = 0, .id = -1 }
} ;

#define MAXOPTS 24
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

static char *print_nlog(char *str, int n)
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
    if (!bprintf(buffer_1,"%s",str) ||
        buffer_putsflush(buffer_1,"\n") == -1)
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_int(uint32_t element)
{
    char ui[UINT_FMT] ;
    ui[uint_fmt(ui, element)] = 0 ;

    if (!buffer_puts(buffer_1, ui))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    if (buffer_putsflush(buffer_1, "\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void info_display_name(char const *field, resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->name) ;
}

static void info_display_version(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->version) ;
}

static void info_display_intree(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->treename) ;
}

static void info_get_status(resolve_service_t *res)
{
    int r, wstat, warn_color = 0 ;
    pid_t pid ;

    ss_state_t sta = STATE_ZERO ;

    if (res->type == TYPE_CLASSIC) {

        r = s6_svc_ok(res->sa.s + res->live.scandir) ;
        if (r != 1) {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            return ;
        }
        char const *newargv[3] ;
        unsigned int m = 0 ;

        newargv[m++] = SS_BINPREFIX "s6-svstat" ;
        newargv[m++] = res->sa.s + res->live.scandir ;
        newargv[m++] = 0 ;

        pid = child_spawn0(newargv[0],newargv,(char const *const *)environ) ;
        if (waitpid_nointr(pid,&wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS,"wait for ",newargv[0]) ;

        if (wstat)
            log_dieu(LOG_EXIT_SYS,"status for service: ",res->sa.s + res->name) ;

    } else {

        char *status = 0 ;

        if (!state_read(&sta, res))
            log_dieusys(LOG_EXIT_SYS,"read state of: ", res->sa.s + res->name) ;

        if (sta.issupervised == STATE_FLAGS_FALSE) {

            status = "unsupervised" ;

        } else if (sta.isup == STATE_FLAGS_FALSE) {

            status = "down" ;
            warn_color = 1 ;

        } else {

            status = "up" ;
            warn_color = 2 ;
        }

        if (!bprintf(buffer_1, "%s%s%s\n", warn_color > 1 ? log_color->valid : warn_color ? log_color->error : log_color->warning, status, log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}

static void info_display_status(char const *field,resolve_service_t *res)
{
    ss_state_t ste = STATE_ZERO ;
    uint32_t disen = 0 ;

    if (NOFIELD) info_display_field_name(field) ;

    if (!state_read(&ste, res))
        log_dieusys(LOG_EXIT_SYS, "read state file of: ", res->sa.s + res->name) ;

    disen = res->enabled ;

    if (!bprintf(buffer_1,"%s%s%s%s", disen ? log_color->valid : log_color->warning, disen ? "enabled" : "disabled", log_color->off, ", "))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    info_get_status(res) ;

}

static void info_display_type(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(get_key_by_enum(list_type, res->type)) ;
}

static void info_display_description(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->description) ;
}

static void info_display_inns(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    char *s = res->inns ? res->sa.s + res->inns : "None" ;
    info_display_string(s) ;
}

static void info_display_notify(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_int(res->notify) ;
}

static void info_display_maxdeath(char const *field, resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_int(res->maxdeath) ;
}

static void info_display_earlier(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_int(res->earlier) ;
}

static void info_display_source(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->path.frontend) ;
}

static void info_display_live(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->live.scandir) ;
}

static void info_display_requiredby(char const *field, resolve_service_t *res)
{
    size_t padding = 1 ;
    int r ;
    graph_t graph = GRAPH_ZERO ;
    struct resolve_hash_s *hres = NULL ;
    stralloc deps = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    service_graph_collect(&graph, res->sa.s + res->name, &hres, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTDOWN) ;

    service_graph_compute(&graph, &hres, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTDOWN) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_requiredby(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get the requiredby list for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, res->sa.s + res->name, 0, &info_graph_display_service, 1, REVERSE, &d, padding, S_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the requiredby list") ;

        goto freed ;

    } else {

        deps.len = 0 ;
        r = graph_matrix_get_edge_g_sorted_sa(&deps,&graph, res->sa.s + res->name, 1, 0) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "get the requiredby list") ;

        if (!r)
            goto empty ;

        if (REVERSE)
            if (!sastr_reverse(&deps))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&deps) ;

        goto freed ;
    }

    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
        else
        {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
    freed:
        graph_free_all(&graph) ;
        hash_free(&hres) ;
        stralloc_free(&deps) ;
}

static void info_display_deps(char const *field, resolve_service_t *res)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;
    struct resolve_hash_s *hres = NULL ;

    stralloc deps = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    service_graph_collect(&graph, res->sa.s + res->name, &hres, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    service_graph_compute(&graph, &hres, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_edge(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get the dependencies list for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH)
    {
        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, res->sa.s + res->name, 0, &info_graph_display_service, 0, REVERSE, &d, padding, S_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

        goto freed ;
    }
    else
    {
        r = graph_matrix_get_edge_g_sorted_sa(&deps,&graph, res->sa.s + res->name, 0, 0) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "get the dependencies list") ;

        if (!r)
            goto empty ;

        if (REVERSE)
            if (!sastr_reverse(&deps))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&deps) ;

        goto freed ;
    }
    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
        else
        {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        hash_free(&hres) ;
        stralloc_free(&deps) ;
}

static void info_display_optsdeps(char const *field, resolve_service_t *res)
{
    stralloc salist = STRALLOC_ZERO ;

    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    if (!res->dependencies.noptsdeps) goto empty ;

    if (!sastr_clean_string(&salist,res->sa.s + res->dependencies.optsdeps))
        log_dieu(LOG_EXIT_SYS,"build optionnal dependencies list") ;

    if (REVERSE)
        if (!sastr_reverse(&salist))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

    info_display_list(field,&salist) ;

    goto freed ;

    empty:
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    freed:
        stralloc_free(&salist) ;
}

static void info_display_contents(char const *field, resolve_service_t *res)
{
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;
    struct resolve_hash_s *hres = NULL ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->dependencies.ncontents)
        goto empty ;

    if (!sastr_clean_string(&sa, res->sa.s + res->dependencies.contents))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!sa.len)
        goto empty ;

    service_graph_g(sa.s, sa.len, &graph, &hres, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","\\"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, 0, 0, &info_graph_display_service, 0, REVERSE, &d, padding, S_STYLE))
            log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

        goto freed ;

    } else {

        if (REVERSE)
            if (!sastr_reverse(&sa))
                log_dieu(LOG_EXIT_SYS,"reverse the selection list") ;

        info_display_list(field,&sa) ;

        goto freed ;
    }
    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","\\"))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", S_STYLE->last, log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }
        else
        {
            if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
        }

    freed:
        graph_free_all(&graph) ;
        hash_free(&hres) ;
        stralloc_free(&sa) ;
}

static void info_display_start(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    size_t padding = info_length_from_wchar(field) + 1 ;
    if (field)
        if (!bprintf(buffer_1,"\n%*s",padding,""))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (res->execute.run.run_user)
        info_display_nline(field, res->sa.s + res->execute.run.run_user) ;
    else
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_stop(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    size_t padding = info_length_from_wchar(field) + 1 ;
    if (field)
        if (!bprintf(buffer_1,"\n%*s",padding,""))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (res->execute.finish.run_user)
        info_display_nline(field,res->sa.s + res->execute.finish.run_user) ;
    else
        if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_envat(char const *field,resolve_service_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    stralloc salink = STRALLOC_ZERO ;

    if (res->environ.envdir) {
        stralloc salink = STRALLOC_ZERO ;
        char *src = res->sa.s + res->environ.envdir ;

        size_t srclen = strlen(src) ;
        char sym[srclen + SS_SYM_VERSION_LEN + 1] ;

        auto_strings(sym,src,SS_SYM_VERSION) ;

        if (sareadlink(&salink, sym) == -1)
            log_dieusys(LOG_EXIT_SYS,"read link of: ",sym) ;

        if (!stralloc_0(&salink))
            log_die_nomem("stralloc") ;

        info_display_string(salink.s) ;

        stralloc_free(&salink) ;

        return ;
    }

    if (!bprintf(buffer_1,"%s%s%s\n",log_color->warning,"None",log_color->off))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

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

    if (res->environ.envdir)
    {
        char *src = res->sa.s + res->environ.envdir ;
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
    if (res->type == TYPE_CLASSIC)
    {
        if (res->logger.name)
        {
            info_display_string(res->sa.s + res->logger.name) ;
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
    if (res->type != TYPE_MODULE)
    {
        if (res->logger.name || (res->type == TYPE_ONESHOT && res->logger.destination))
        {
            info_display_string(res->sa.s + res->logger.destination) ;
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
    if (res->type != TYPE_MODULE)
    {
        if (res->logger.name || (res->type == TYPE_ONESHOT && res->logger.destination))
        {
            if (nlog)
            {
                stralloc log = STRALLOC_ZERO ;
                /** the file current may not exist if the service was never started*/
                size_t dstlen = strlen(res->sa.s + res->logger.destination) ;
                char scan[dstlen + 9] ;
                memcpy(scan,res->sa.s + res->logger.destination,dstlen) ;
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
                    if (!file_readputsa(&log,res->sa.s + res->logger.destination,"current")) log_dieusys(LOG_EXIT_SYS,"read log file of: ",res->sa.s + res->name) ;
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
    unsigned int nopts = 0 , old = 0 ;
    info_opts_map_t const *t ;
    _alloc_stk_(stk, strlen(str) + 1) ;

    if (!lexer_trim_with_delim(&stk,str,DELIM))
        log_dieu(LOG_EXIT_SYS,"parse options") ;

    checkopts(stk.count) ;

    FOREACH_STK(&stk, pos) {

        char *o = stk.s + pos ;
        t = opts_sv_table ;
        old = nopts ;
        for (; t->str; t++) {

            if (!strcmp(o,t->str))
                what[nopts++] = t->id ;
        }

        if (old == nopts)
            log_die(LOG_EXIT_SYS,"invalid option: ",o) ;
    }
}

void info_status_all(void)
{
    _alloc_sa_(sa) ;
    struct resolve_hash_tree_s *htres = NULL ;
    graph_t graph = GRAPH_ZERO ;

    graph_build_tree(&graph, &htres, pinfo->base.s, E_RESOLVE_TREE_MASTER_CONTENTS) ;

    if (!graph_matrix_sort_tosa(&sa, &graph))
        log_dieu(LOG_EXIT_SYS, "get the sorted list of trees") ;

    graph_free_all(&graph) ;

    if (sa.len) {

        struct resolve_hash_tree_s *c, *tmp ;
        struct resolve_hash_s *hres = NULL ;

        HASH_ITER(hh, htres, c, tmp) {

            if (c->tres.ncontents) {

                _alloc_stk_(stk, strlen(c->tres.sa.s + c->tres.contents)) ;

                if (!stack_string_clean(&stk, c->tres.sa.s + c->tres.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                service_graph_g(stk.s, stk.len, &graph, &hres, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP|STATE_FLAGS_WANTDOWN) ;

                if (!graph.mlen)
                    log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

                if (!bprintf(buffer_1,"%s%s%s%s\n","In tree: ", log_color->info, c->tres.sa.s + c->tres.name, log_color->off))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
                if (!bprintf(buffer_1,"%s\n","\\"))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

                depth_t d = info_graph_init() ;

                if (!info_walk(&graph, 0, 0, &info_graph_display_service, 0, REVERSE, &d, 0, S_STYLE))
                    log_dieu(LOG_EXIT_SYS,"display the dependencies list") ;

                if (buffer_puts(buffer_1,"\n") == -1)
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

                graph_free_all(&graph) ;
                hash_free(&hres) ;
            }
        }

    } else {
        log_dieusys(LOG_EXIT_SYS, "find trees -- please make a bug report") ;
    }

    hash_free_tree(&htres) ;
}

void info_status_one(const char *service, int *what)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    int r = service_is_g(service, STATE_FLAGS_ISPARSED) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", service, " -- please make a bug report") ;

    if (!r || r == STATE_FLAGS_FALSE)
        log_die(LOG_EXIT_SYS, "service: ", service, " is not parsed -- try to parse it using '66 parse ", service, "'") ;

    if (resolve_read_g(wres, pinfo->base.s, service) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", service) ;

    info_display_all(&res, what) ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    resolve_free(wres) ;

}

int ssexec_status(int argc, char const *const *argv, ssexec_t *info)
{
    short legacy = 1, all = 0 ;
    int what[MAXOPTS] = { 0 } ;

    pinfo = info ;

    char const *svname = 0 ;

    for (int i = 0 ; i < MAXOPTS ; i++)
        what[i] = -1 ;

    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "Name",
        "Version" ,
        "In tree",
        "Status",
        "Type",
        "Description",
        "Part of",
        "Notify",
        "Max death",
        "Earlier",
        "Source",
        "Live",
        "Dependencies",
        "Required by",
        "Contents",
        "Optional dependencies" ,
        "Start script",
        "Stop script",
        "Environment source",
        "Environment file",
        "Log name",
        "Log destination",
        "Log file" } ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, OPTS_STATUS, &l) ;
            if (opt == -1) break ;

            switch (opt) {
                case 'h' :  info_help(info->help, info->usage) ; return 0 ;
                case 'n' :  NOFIELD = 0 ; break ;
                case 'o' :  legacy = 0 ; info_parse_options(l.arg,what) ; break ;
                case 'g' :  GRAPH = 1 ; break ;
                case 'r' :  REVERSE = 1 ; break ;
                case 'd' :  if (!uint0_scan(l.arg, &MAXDEPTH)) log_usage(info->usage, "\n", info->help) ; break ;
                case 'p' :  if (!uint0_scan(l.arg, &nlog)) log_usage(info->usage, "\n", info->help) ; break ;
                default :   log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        all = 1 ;

    svname = *argv ;

    if (legacy) {

        unsigned int i = 0 ;
        for (; i < MAXOPTS - 1 ; i++)
            what[i] = i ;

        what[i] = -1 ;
    }

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    setlocale(LC_ALL, "");

    if(!strcmp(nl_langinfo(CODESET), "UTF-8"))
        S_STYLE = &graph_utf8;

    if (!all) {
        info_status_one(svname, what) ;
    } else {
        info_status_all() ;
    }

    return 0 ;

}
