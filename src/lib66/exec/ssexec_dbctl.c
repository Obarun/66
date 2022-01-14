/*
 * ssexec_dbctl.c
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
#include <sys/types.h>//pid_t
//#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/tai.h>
#include <skalibs/types.h>//UINT_FMT
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>
#include <s6-rc/config.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/db.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/service.h>

static unsigned int DEADLINE = 0 ;

static void rebuild_list(ss_resolve_graph_t *graph,ssexec_t *info, int what)
{
    log_flow() ;

    int isup ;
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;
    genalloc gatmp = GENALLOC_ZERO ;
    ss_state_t sta = STATE_ZERO ;

    for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&graph->sorted) ; i++)
    {
        char *string = genalloc_s(resolve_service_t,&graph->sorted)[i].sa.s ;
        char *name = string + genalloc_s(resolve_service_t,&graph->sorted)[i].name ;
        char *runat = string + genalloc_s(resolve_service_t,&graph->sorted)[i].runat ;
        char *state = string + genalloc_s(resolve_service_t,&graph->sorted)[i].state ;
        if (!state_check(state,name)) log_die(LOG_EXIT_SYS,"unitialized service: ",name) ;
        if (!state_read(&sta,state,name)) log_dieusys(LOG_EXIT_SYS,"read state of: ",name) ;
        if (sta.init) log_die(LOG_EXIT_SYS,"unitialized service: ",name) ;

        int type = genalloc_s(resolve_service_t,&graph->sorted)[i].type ;
        if (type == TYPE_LONGRUN && genalloc_s(resolve_service_t,&graph->sorted)[i].disen)
        {
            if (!s6_svstatus_read(runat,&status)) log_dieusys(LOG_EXIT_SYS,"read status of: ",runat) ;
            isup = status.pid && !status.flagfinishing ;

            if (isup && !what)
            {
                log_info("Already up: ",name) ;
                continue ;
            }
            else if (!isup && what)
            {
                log_info("Already down: ",name) ;
                continue ;
            }
        }
        else
        {
            if (!sta.state && what || !genalloc_s(resolve_service_t,&graph->sorted)[i].disen)
            {
                log_info("Already down: ",name) ;
                continue ;
            }
            if (sta.state && !what)
            {
                log_info("Already up: ",name) ;
                continue ;
            }
        }
        genalloc_append(resolve_service_t,&gatmp,&genalloc_s(resolve_service_t,&graph->sorted)[i]) ;
    }
    genalloc_copy(resolve_service_t,&graph->sorted,&gatmp) ;
    genalloc_free(resolve_service_t,&gatmp) ;
}

/* signal = 0 -> reload
 * signal = 1 -> up
 * signal > 1 -> down*/
static int check_status(genalloc *gares,ssexec_t *info,int signal)
{
    log_flow() ;

    int reload = 0 , up = 0 , ret = 0 ;

    s6_svstatus_t status = S6_SVSTATUS_ZERO ;
    ss_state_t sta = STATE_ZERO ;
    if (!signal) reload = 1 ;
    else if (signal == 1) up = 1 ;

    for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,gares) ; i++)
    {
        int nret = 0 ;
        resolve_service_t_ref pres = &genalloc_s(resolve_service_t,gares)[i] ;
        char const *name = pres->sa.s + pres->name ;
        char const *state = pres->sa.s + pres->state ;
        /** do not touch the Master resolve file*/
        if (obstr_equal(name,SS_MASTER + 1)) continue ;
        /** only check longrun service */
        if (pres->type == TYPE_LONGRUN)
        {
            if (!s6_svstatus_read(pres->sa.s + pres->runat,&status)) log_dieusys(LOG_EXIT_SYS,"read status of: ",pres->sa.s + pres->runat) ;
            else if (up)
            {
                if ((!WEXITSTATUS(status.wstat) && !WIFSIGNALED(status.wstat)) || (WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat) && (WTERMSIG(status.wstat) == 15 )))
                {
                    state_setflag(&sta,SS_FLAGS_PID,status.pid) ;
                    state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
                }
                else
                {
                    log_warnu("start: ",name) ;
                    nret = 1 ;
                    state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
                    state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
                }
            }
            else
            {
                if ((!WEXITSTATUS(status.wstat) && !WIFSIGNALED(status.wstat)) || (WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat) && (WTERMSIG(status.wstat) == 15 )))
                {
                    state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
                    state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
                }
                else
                {
                    log_warnu("stop: ",name) ;
                    state_setflag(&sta,SS_FLAGS_PID,status.pid) ;
                    state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
                    nret = 1 ;
                }
            }
        }
        if (nret) ret = 111 ;
        state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
        state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
    //  state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
        if (pres->type == TYPE_BUNDLE || pres->type == TYPE_ONESHOT)
        {
            if (up) state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
            else state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
        }
        log_trace("Write state file of: ",name) ;
        if (!state_write(&sta,state,name))
        {
            log_warnusys("write state file of: ",name) ;
            ret = 111 ;
        }
        if (!nret) log_info(reload ? "Reloaded" : up ? "Started" : "Stopped"," successfully: ",name) ;
    }

    return ret ;
}
static pid_t send(genalloc *gasv, char const *livetree, char const *signal,char const *const *envp)
{
    log_flow() ;

    tain deadline ;
    tain_from_millisecs(&deadline, DEADLINE) ;

    tain_now_g() ;
    tain_add_g(&deadline, &deadline) ;

    char const *newargv[10 + genalloc_len(resolve_service_t,gasv)] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;

    char tt[UINT32_FMT] ;
    tt[uint32_fmt(tt,DEADLINE)] = 0 ;

    newargv[m++] = S6RC_BINPREFIX "s6-rc" ;
    newargv[m++] = "-v" ;
    newargv[m++] = fmt ;
    newargv[m++] = "-t" ;
    newargv[m++] = tt ;
    newargv[m++] = "-l" ;
    newargv[m++] = livetree ;
    newargv[m++] = signal ;
    newargv[m++] = "change" ;

    for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,gasv); i++)
        newargv[m++] = genalloc_s(resolve_service_t,gasv)[i].sa.s + genalloc_s(resolve_service_t,gasv)[i].name  ;

    newargv[m++] = 0 ;

    return child_spawn0(newargv[0],newargv,envp) ;

}

int ssexec_dbctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    DEADLINE = 0 ;

    if (info->timeout) DEADLINE = info->timeout ;

    unsigned int up, down, reload, ret, reverse ;

    int r, wstat ;
    pid_t pid ;

    char *signal = 0 ;
    char *mainsv = SS_MASTER + 1 ;

    genalloc gares = GENALLOC_ZERO ; //resolve_service_t
    stralloc tmp = STRALLOC_ZERO ;
    stralloc sares = STRALLOC_ZERO ;
    ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    up = down = reload = ret = reverse = 0 ;

    //PROG = "66-dbctl" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, OPTS_DBCTL, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'u' :  up = 1 ; if (down || reload) log_usage(usage_dbctl) ; break ;
                case 'd' :  down = 1 ; if (up || reload) log_usage(usage_dbctl) ; break ;
                case 'r' :  reload = 1 ; if (down || up) log_usage(usage_dbctl) ; break ;
                default : log_usage(usage_dbctl) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!up && !down && !reload){ log_warn("signal must be set") ; log_usage(usage_dbctl) ; }

    if (down)
    {
        signal = "-d" ;
    }
    else signal = "-u" ;

    if (!sa_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) log_dieusys(LOG_EXIT_SYS,"set revolve pointer to source") ;

    if (argc < 1)
    {

        if (!resolve_check(sares.s,mainsv)) log_diesys(LOG_EXIT_SYS,"inner bundle doesn't exit -- please make a bug report") ;
        if (!resolve_read(wres,sares.s,mainsv)) log_dieusys(LOG_EXIT_SYS,"read resolve file of inner bundle") ;
        if (res.ndepends)
        {
            if (!resolve_append(&gares,wres)) log_dieusys(LOG_EXIT_SYS,"append services selection with inner bundle") ;
        }
        else
        {
            log_info("nothing to do") ;
            resolve_free(wres) ;
            goto freed ;
        }

    }
    else
    {
        for(;*argv;argv++)
        {
            char const *name = *argv ;
            if (!resolve_check(sares.s,name)) log_diesys(LOG_EXIT_SYS,"unknown service: ",name) ;
            if (!resolve_read(wres,sares.s,name)) log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;
            if (res.type == TYPE_CLASSIC) log_die(LOG_EXIT_SYS,name," has type classic") ;
            if (!resolve_append(&gares,wres)) log_dieusys(LOG_EXIT_SYS,"append services selection with: ", name) ;
        }
    }

    if (!db_ok(info->livetree.s,info->treename.s))
        log_diesys(LOG_EXIT_SYS,"db: ",info->livetree.s,"/",info->treename.s," is not running") ;

    if (!stralloc_cats(&tmp,info->livetree.s)) log_die_nomem("stralloc") ;
    if (!stralloc_cats(&tmp,"/")) log_die_nomem("stralloc") ;
    if (!stralloc_cats(&tmp,info->treename.s)) log_die_nomem("stralloc") ;
    if (!stralloc_0(&tmp)) log_die_nomem("stralloc") ;

    if (reload)
    {
        reverse = 1 ;
        for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&gares) ; i++)
        {
            if (!ss_resolve_graph_build(&graph,&genalloc_s(resolve_service_t,&gares)[i],sares.s,reverse)) log_dieusys(LOG_EXIT_SYS,"build services graph") ;
        }
        r = ss_resolve_graph_publish(&graph,reverse) ;
        if (r < 0) log_die(LOG_EXIT_SYS,"cyclic dependencies detected") ;
        if (!r) log_dieusys(LOG_EXIT_SYS,"publish service graph") ;

        rebuild_list(&graph,info,reverse) ;

        pid = send(&graph.sorted,tmp.s,"-d",envp) ;

        if (waitpid_nointr(pid,&wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS,"wait for s6-rc") ;

        if (wstat) log_dieu(LOG_EXIT_SYS," stop services selection") ;

        ret = check_status(&graph.sorted,info,2) ;
        if (ret) goto freed ;
        ss_resolve_graph_free(&graph) ;
    }

    if (down) reverse = 1 ;
    else reverse = 0 ;

    for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&gares) ; i++)
    {
        int ireverse = reverse ;
        int logname = get_rstrlen_until(genalloc_s(resolve_service_t,&gares)[i].sa.s + genalloc_s(resolve_service_t,&gares)[i].name,SS_LOG_SUFFIX) ;
        if (logname > 0 && (!resolve_cmp(&gares,genalloc_s(resolve_service_t,&gares)[i].sa.s + genalloc_s(resolve_service_t,&gares)[i].logassoc, DATA_SERVICE)) && down)
            ireverse = 1  ;

        if (reload) ireverse = 1 ;
        if (!ss_resolve_graph_build(&graph,&genalloc_s(resolve_service_t,&gares)[i],sares.s,ireverse)) log_dieusys(LOG_EXIT_SYS,"build services graph") ;
    }
    r = ss_resolve_graph_publish(&graph,reverse) ;
    if (r < 0) log_die(LOG_EXIT_SYS,"cyclic dependencies detected") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"publish service graph") ;

    rebuild_list(&graph,info,reverse) ;

    pid = send(&graph.sorted,tmp.s,signal,envp) ;

    if (waitpid_nointr(pid,&wstat, 0) < 0)
        log_dieusys(LOG_EXIT_SYS,"wait for s6-rc") ;

    if (wstat) log_dieu(LOG_EXIT_SYS,down ? "stop" : "start"," services selection") ;

    ret = check_status(&graph.sorted,info,down ? 2 : 1) ;

    freed:
    stralloc_free(&tmp) ;
    stralloc_free(&sares) ;
    ss_resolve_graph_free(&graph) ;
    resolve_free(wres) ;
    resolve_deep_free(DATA_SERVICE, &gares) ;

    return ret ;
}
