/*
 * ssexec_inservice.c
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
#include <stdlib.h>

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
#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>

#include <s6/supervise.h>

/**
 *
 *
 *
 *
 * a revoir, notamment les fonctions appeler comme optsdeps
 *le tname pour le tree etc etc
 *
 *
 *
 *
 *
 *
 *
 *
 * */
static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
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
static void info_display_start(char const *field, resolve_service_t *res) ;
static void info_display_stop(char const *field, resolve_service_t *res) ;
static void info_display_envat(char const *field, resolve_service_t *res) ;
static void info_display_envfile(char const *field, resolve_service_t *res) ;
static void info_display_logname(char const *field, resolve_service_t *res) ;
static void info_display_logdst(char const *field, resolve_service_t *res) ;
static void info_display_logfile(char const *field, resolve_service_t *res) ;

info_graph_style *S_STYLE = &graph_default ;

static ssexec_t_ref pinfo = 0 ;

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
    { .str = "optsdepends", .svfunc = &info_display_optsdeps, .id = 10 },
    { .str = "start", .svfunc = &info_display_start, .id = 11 },
    { .str = "stop", .svfunc = &info_display_stop, .id = 12 },
    { .str = "envat", .svfunc = &info_display_envat, .id = 13 },
    { .str = "envfile", .svfunc = &info_display_envfile, .id = 14 },
    { .str = "logname", .svfunc = &info_display_logname, .id = 15 },
    { .str = "logdst", .svfunc = &info_display_logdst, .id = 16 },
    { .str = "logfile", .svfunc = &info_display_logfile, .id = 17 },
    { .str = 0, .svfunc = 0, .id = -1 }
} ;

#define MAXOPTS 19
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many options")
#define DELIM ','

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
    if (!bprintf(buffer_1,"%s",str) ||
        buffer_putsflush(buffer_1,"\n") == -1)
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
    info_display_string(res->sa.s + res->version) ;
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

        char *ste = res->sa.s + res->path.home ;
        char *name = res->sa.s + res->name ;
        char *status = 0 ;

        if (!state_read(&sta, ste, name))
            log_dieusys(LOG_EXIT_SYS,"read state of: ",name) ;

        if (!service_is(&sta, STATE_FLAGS_TOINIT)) {

            status = "unitialized" ;

        } else if (!service_is(&sta, STATE_FLAGS_ISUP)) {

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

    if (!state_read(&ste, res->sa.s + res->path.home, res->sa.s + res->name))
        log_dieusys(LOG_EXIT_SYS, "read state file of: ", res->sa.s + res->name) ;

    disen = service_is(&ste, STATE_FLAGS_ISENABLED) ;

    if (!bprintf(buffer_1,"%s%s%s%s", disen ? log_color->valid : log_color->error, disen ? "enabled" : "disabled", log_color->off, ", "))
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

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    stralloc deps = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    graph_build_service(&graph, ares, &areslen, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

   if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_requiredby(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get the requiredby list for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH) {

        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, res->sa.s + res->name, res->sa.s + res->treename, &info_graph_display_service, 1, REVERSE, &d, padding, S_STYLE))
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
                log_dieu(LOG_EXIT_SYS,"reverse the requiredby list") ;

        info_display_list(field,&deps) ;

        goto freed ;
    }

    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","/"))
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
        stralloc_free(&deps) ;
}

static void info_display_deps(char const *field, resolve_service_t *res)
{
    int r ;
    size_t padding = 1 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    stralloc deps = STRALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    graph_build_service(&graph, ares, &areslen, pinfo, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    unsigned int list[graph.mlen] ;

    int count = graph_matrix_get_edge(list, &graph, res->sa.s + res->name , 0) ;

    if (count == -1)
        log_dieu(LOG_EXIT_SYS,"get the dependencies list for service: ", res->sa.s + res->name) ;

    if (!count) goto empty ;

    if (GRAPH)
    {
        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        depth_t d = info_graph_init() ;

        if (!info_walk(&graph, res->sa.s + res->name, res->sa.s + res->treename, &info_graph_display_service, 0, REVERSE, &d, padding, S_STYLE))
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
                log_dieu(LOG_EXIT_SYS,"reverse the dependencies list") ;

        info_display_list(field,&deps) ;

        goto freed ;
    }
    empty:
        if (GRAPH)
        {
            if (!bprintf(buffer_1,"%s\n","/"))
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
        stralloc_free(&deps) ;
}

static void info_display_with_source_tree(stralloc *list,resolve_service_t *res)
{
    size_t pos = 0, lpos = 0, newlen = 0 ;
    stralloc svlist = STRALLOC_ZERO ;
    stralloc ntree = STRALLOC_ZERO ;
    stralloc src = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *exclude[2] = { SS_RESOLVE + 1, 0 } ;
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

    if (!res->dependencies.noptsdeps) goto empty ;

    if (!sastr_clean_string(&salist,res->sa.s + res->dependencies.optsdeps))
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

    if (!res->environ.envdir) goto empty ;
    {
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
    if (res->type != TYPE_BUNDLE || res->type != TYPE_MODULE)
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
    if (res->type != TYPE_BUNDLE || res->type != TYPE_MODULE)
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

int ssexec_inservice(int argc, char const *const *argv, ssexec_t *info)
{
    unsigned int legacy = 1 ;
    int r = 0 ;
    int what[MAXOPTS] = { 0 } ;

    pinfo = info ;
    uid_t owner ;
    char ownerstr[UID_FMT] ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    stralloc satree = STRALLOC_ZERO ;

    char const *svname = 0 ;
    char atree[SS_MAX_TREENAME + 1] ;

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
            int opt = subgetopt_r(argc,argv, OPTS_INSERVICE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'n' :  NOFIELD = 0 ; break ;
                case 'o' :  legacy = 0 ; info_parse_options(l.arg,what) ; break ;
                case 'g' :  GRAPH = 1 ; break ;
                case 'r' :  REVERSE = 1 ; break ;
                case 'd' :  if (!uint0_scan(l.arg, &MAXDEPTH)) log_usage(usage_service_status, "\n", help_service_status) ; break ;
                case 'p' :  if (!uint0_scan(l.arg, &nlog)) log_usage(usage_service_status, "\n", help_service_status) ; break ;
                default :   log_usage(usage_service_status, "\n", help_service_status) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc) log_usage(usage_service_status, "\n", help_service_status) ;
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
        S_STYLE = &graph_utf8;
    }
    if (!set_ownersysdir(&home,owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
    if (!auto_stra(&home,SS_SYSTEM,"/")) log_die_nomem("stralloc") ;

    r = service_is_g(atree, svname, STATE_FLAGS_ISPARSED) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;

    if (!r) {
        /** nothing to do */
        log_1_warn("unknown service: ", svname) ;
        goto freed ;
    }

    if (!resolve_read_g(wres, info->base.s, svname))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", svname) ;

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
