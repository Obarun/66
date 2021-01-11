/*
 * 66-inservice.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>

#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/environ.h>
#include <66/state.h>

#include <s6/s6-supervise.h>

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
static void info_display_name(char const *field, ss_resolve_t *res) ;
static void info_display_intree(char const *field, ss_resolve_t *res) ;
static void info_display_status(char const *field, ss_resolve_t *res) ;
static void info_display_type(char const *field, ss_resolve_t *res) ;
static void info_display_description(char const *field, ss_resolve_t *res) ;
static void info_display_version(char const *field, ss_resolve_t *res) ;
static void info_display_source(char const *field, ss_resolve_t *res) ;
static void info_display_live(char const *field, ss_resolve_t *res) ;
static void info_display_deps(char const *field, ss_resolve_t *res) ;
static void info_display_optsdeps(char const *field, ss_resolve_t *res) ;
static void info_display_extdeps(char const *field, ss_resolve_t *res) ;
static void info_display_start(char const *field, ss_resolve_t *res) ;
static void info_display_stop(char const *field, ss_resolve_t *res) ;
static void info_display_envat(char const *field, ss_resolve_t *res) ;
static void info_display_envfile(char const *field, ss_resolve_t *res) ;
static void info_display_logname(char const *field, ss_resolve_t *res) ;
static void info_display_logdst(char const *field, ss_resolve_t *res) ;
static void info_display_logfile(char const *field, ss_resolve_t *res) ;

ss_resolve_graph_style *STYLE = &graph_default ;

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
    { .str = "extdepends", .svfunc = &info_display_extdeps, .id = 9 },
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

#define USAGE "66-inservice [ -h ] [ -z ] [ -v verbosity ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -t tree ] [ -p nline ] service"

static inline void info_help (void)
{
  static char const *help =
"66-inservice <options> service \n"
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
"valid field for -o options are:\n"
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

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
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

    while (ndelim < n)
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

static void info_display_name(char const *field, ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->name) ;
}

static void info_display_version(char const *field,ss_resolve_t *res)
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

static void info_display_intree(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->treename) ;
}

static void info_get_status(ss_resolve_t *res)
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
        if (!ss_state_check(ste,name))
        {
            status = "unitialized" ;
            goto dis ;
        }

        if (!ss_state_read(&sta,ste,name))
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

static void info_display_status(char const *field,ss_resolve_t *res)
{

    if (NOFIELD) info_display_field_name(field) ;

    if (!bprintf(buffer_1,"%s%s%s%s",res->disen ? log_color->valid : log_color->error,res->disen ? "enabled" : "disabled",log_color->off,", "))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    if (buffer_putsflush(buffer_1,"") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    info_get_status(res) ;

}

static void info_display_type(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(get_key_by_enum(ENUM_TYPE,res->type)) ;
}

static void info_display_description(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->description) ;
}

static void info_display_source(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->src) ;
}

static void info_display_live(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    info_display_string(res->sa.s + res->runat) ;
}

static void info_display_deps(char const *field, ss_resolve_t *res)
{
    int r ;
    size_t padding = 1, pos = 0, el ;
    ss_resolve_t gres = RESOLVE_ZERO ;
    ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
    stralloc salist = STRALLOC_ZERO ;
    genalloc gares = GENALLOC_ZERO ;

    if (NOFIELD) padding = info_display_field_name(field) ;
    else { field = 0 ; padding = 0 ; }

    if (!res->ndeps) goto empty ;

    if (res->type == TYPE_MODULE)
    {
        if (!sastr_clean_string(&salist,res->sa.s + res->contents))
            log_dieu(LOG_EXIT_SYS,"rebuild dependencies list") ;

        if (!ss_resolve_sort_bytype(&gares,&salist,src.s))
            log_dieu(LOG_EXIT_SYS,"sort list by type") ;

        for (pos = 0 ; pos < genalloc_len(ss_resolve_t,&gares) ; pos++)
            if (!ss_resolve_graph_build(&graph,&genalloc_s(ss_resolve_t,&gares)[pos],src.s,REVERSE))
                log_dieu(LOG_EXIT_SYS,"build the graph from: ",src.s) ;

        r = ss_resolve_graph_publish(&graph,0) ;
        if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected") ;
        else if (!r) log_dieusys(LOG_EXIT_SYS,"publish service graph") ;

        salist.len = 0 ;
        for (pos = 0 ; pos < genalloc_len(ss_resolve_t,&graph.sorted) ; pos++)
        {
            char *string = genalloc_s(ss_resolve_t,&graph.sorted)[pos].sa.s ;
            char *name = string + genalloc_s(ss_resolve_t,&graph.sorted)[pos].name ;
            if (!stralloc_catb(&salist,name,strlen(name)+1)) log_die_nomem("stralloc") ;
        }
        genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
        ss_resolve_graph_free(&graph) ;
    }
    else {
        if (!sastr_clean_string(&salist,res->sa.s + res->deps))
            log_dieu(LOG_EXIT_SYS,"rebuild dependencies list") ;
    }

    if (GRAPH)
    {
        if (!bprintf(buffer_1,"%s\n","/"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

        el = sastr_len(&salist) ;
        if (!sastr_rebuild_in_oneline(&salist)) log_dieu(LOG_EXIT_SYS,"rebuild dependencies list") ;

        ss_resolve_init(&gres) ;
        gres.ndeps = el ;
        gres.deps = ss_resolve_add_string(&gres,salist.s) ;

        if (!info_graph_init(&gres,src.s,REVERSE, padding, STYLE))
            log_dieu(LOG_EXIT_SYS,"display graph of: ",gres.sa.s + gres.name) ;

        goto freed ;
    }
    else
    {
        if (REVERSE)
            if (!sastr_reverse(&salist))
                log_dieu(LOG_EXIT_SYS,"reverse dependencies list") ;
        info_display_list(field,&salist) ;
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
        ss_resolve_free(&gres) ;
        stralloc_free(&salist) ;
}

static void info_display_with_source_tree(stralloc *list,ss_resolve_t *res)
{
    size_t pos = 0, lpos = 0, newlen = 0 ;
    stralloc svlist = STRALLOC_ZERO ;
    stralloc ntree = STRALLOC_ZERO ;
    stralloc src = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char *treename = 0 ;

    if (!auto_stra(&src,home.s)) log_die_nomem("stralloc") ;
    newlen = src.len ;

    if (!sastr_dir_get(&ntree,home.s,SS_BACKUP + 1,S_IFDIR))
        log_dieu(LOG_EXIT_SYS,"get list of trees of: ",home.s) ;

    for (pos = 0 ; pos < ntree.len ; pos += strlen(ntree.s + pos) + 1)
    {
        svlist.len = 0 ;
        src.len = newlen ;
        treename = ntree.s + pos ;

        if (!auto_stra(&src,treename,SS_SVDIRS,SS_RESOLVE))
            log_die_nomem("stralloc") ;

        if (!sastr_dir_get(&svlist,src.s,SS_MASTER + 1,S_IFREG))
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

static void info_display_optsdeps(char const *field, ss_resolve_t *res)
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

static void info_display_extdeps(char const *field, ss_resolve_t *res)
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

static void info_display_start(char const *field,ss_resolve_t *res)
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

static void info_display_stop(char const *field,ss_resolve_t *res)
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

static void info_display_envat(char const *field,ss_resolve_t *res)
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

static void info_display_envfile(char const *field,ss_resolve_t *res)
{
    if (NOFIELD) info_display_field_name(field) ;
    else field = 0 ;

    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    stralloc salink = STRALLOC_ZERO ;
    stralloc list = STRALLOC_ZERO ;

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

        if (!sastr_dir_get(&list,salink.s,"",S_IFREG))
            log_dieusys(LOG_EXIT_SYS,"get list of environment file from: ",src) ;

        if (!sastr_sort(&list))
            log_dieu(LOG_EXIT_SYS,"sort environment file name") ;

        FOREACH_SASTR(&list,pos) {

            sa.len = 0 ;
            salink.len = newlen ;
            if (!stralloc_cats(&salink,"/") ||
            !stralloc_cats(&salink,list.s + pos) ||
            !stralloc_0(&salink)) log_die_nomem("stralloc") ;

            if (!file_readputsa_g(&sa,salink.s))
                log_dieusys(LOG_EXIT_SYS,"read environment file") ;

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

static void info_display_logname(char const *field,ss_resolve_t *res)
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

static void info_display_logdst(char const *field,ss_resolve_t *res)
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

static void info_display_logfile(char const *field,ss_resolve_t *res)
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

static void info_display_all(ss_resolve_t *res,int *what)
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

    ss_resolve_t res = RESOLVE_ZERO ;
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
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hzv:cno:grd:t:p:", &l) ;
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
                case 't' :  tname = l.arg ; break ;
                case 'p' :  if (!uint0_scan(l.arg, &nlog)) log_usage(USAGE) ; break ;
                case 'c' :  log_die(LOG_EXIT_SYS,"deprecated option -- please use -z instead") ;
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

    if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
        STYLE = &graph_utf8;
    }
    if (!set_ownersysdir(&home,owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
    if (!auto_stra(&home,SS_SYSTEM,"/")) log_die_nomem("stralloc") ;

    found = ss_resolve_svtree(&src,svname,tname) ;

    if (found == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of sv: ",svname) ;
    else if (!found) {
        log_info("no tree exist yet") ;
        goto freed ;
    }
    else if (found > 2) {
        log_die(LOG_EXIT_SYS,svname," is set on different tree -- please use -t options") ;
    }
    else if (found == 1) log_die(LOG_EXIT_SYS,"unknown service: ",svname) ;

    if (!ss_resolve_read(&res,src.s,svname))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",svname) ;

    info_display_all(&res,what) ;

    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


    freed:
    ss_resolve_free(&res) ;
    stralloc_free(&src) ;
    stralloc_free(&home) ;
    stralloc_free(&satree) ;

    return 0 ;

}
