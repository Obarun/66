/*
 * ssexec_stop.c
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

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/svc.h>
#include <66/rc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/state.h>

static unsigned int DEADLINE = 0 ;
static unsigned int UNSUP = 0 ;
static char *SIG = "-d" ;

static ss_resolve_graph_t graph_unsup_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_unsup_rc = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_rc = RESOLVE_GRAPH_ZERO ;

int svc_down(ssexec_t *info, char const *const *envp)
{
    log_flow() ;

    unsigned int reverse = 1 ;
    int r ;

    if (genalloc_len(ss_resolve_t,&graph_unsup_cl.name))
    {
        UNSUP = 1 ;
        r = ss_resolve_graph_publish(&graph_unsup_cl,reverse) ;
        if (r < 0 || !r)
        {
            log_warnusys("publish service graph") ;
            goto err ;
        }
        if (!svc_unsupervise(info,&graph_unsup_cl.sorted,SIG,envp)) goto err ;
    }
    else
    {
        r = ss_resolve_graph_publish(&graph_cl,reverse) ;
        if (r < 0 || !r)
        {
            log_warnusys("publish service graph") ;
            goto err ;
        }
        if (!svc_send(info,&graph_cl.sorted,SIG,envp)) goto err ;
    }
    ss_resolve_graph_free(&graph_unsup_cl) ;
    ss_resolve_graph_free(&graph_cl) ;
    return 1 ;
    err:
        ss_resolve_graph_free(&graph_unsup_cl) ;
        ss_resolve_graph_free(&graph_cl) ;
        return 0 ;
}

int rc_down(ssexec_t *info, char const *const *envp)
{
    log_flow() ;

    unsigned int reverse = 1 ;
    int r ;

    if (genalloc_len(ss_resolve_t,&graph_unsup_rc.name))
    {
        UNSUP = 1 ;
        r = ss_resolve_graph_publish(&graph_unsup_rc,reverse) ;
        if (r < 0 || !r)
        {
            log_warnusys("publish service graph") ;
            goto err ;
        }
        if (!rc_unsupervise(info,&graph_unsup_rc.sorted,"-d",envp)) goto err ;
    }
    else
    {
        r = ss_resolve_graph_publish(&graph_rc,reverse) ;
        if (r < 0 || !r)
        {
            log_warnusys("publish service graph") ;
            goto err ;
        }
        if (!rc_send(info,&graph_rc.sorted,"-d",envp)) goto err ;
    }

    ss_resolve_graph_free(&graph_unsup_rc) ;
    ss_resolve_graph_free(&graph_rc) ;
    return 1 ;
    err:
        ss_resolve_graph_free(&graph_unsup_rc) ;
        ss_resolve_graph_free(&graph_rc) ;
        return 0 ;
}

int ssexec_stop(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    // be sure that the global var are set correctly
    DEADLINE = 0 ;
    UNSUP = 0 ;
    SIG = "-d" ;

    if (info->timeout) DEADLINE = info->timeout ;

    int cl, rc, sigopt ;
    stralloc sares = STRALLOC_ZERO ;
    genalloc gares = GENALLOC_ZERO ; //ss_resolve_t
    ss_resolve_t_ref pres ;
    ss_resolve_t res = RESOLVE_ZERO ;
    ss_state_t sta = STATE_ZERO ;

    cl = rc = sigopt = 0 ;

    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">uXK", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;

            switch (opt)
            {
                case 'u' :  UNSUP = 1 ; break ;
                case 'X' :  if (sigopt) log_usage(usage_stop) ; sigopt = 1 ; SIG = "-X" ; break ;
                case 'K' :  if (sigopt) log_usage(usage_stop) ; sigopt = 1 ; SIG = "-K" ; break ;
                default :   log_usage(usage_stop) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(usage_stop) ;

    if ((scandir_ok(info->scandir.s)) !=1 ) log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) log_dieusys(LOG_EXIT_SYS,"set revolve pointer to source") ;

    for (;*argv;argv++)
    {
        char const *name = *argv ;
        if (!ss_resolve_check(sares.s,name)) log_info_return(LOG_EXIT_ZERO,name," is not enabled") ;
        if (!ss_resolve_read(&res,sares.s,name)) log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;
        if (res.type == TYPE_MODULE)
        {
            if (!module_in_cmdline(&gares,&res,sares.s))
                log_dieu(LOG_EXIT_SYS,"add dependencies of module: ",name) ;
        }
        else
        {
            if (!ss_resolve_append(&gares,&res))
                log_dieusys(LOG_EXIT_SYS,"append resolve file of: ",name) ;
        }
    }

    for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
    {
        int unsup = 0 , reverse = 1 ;
        pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
        char const *string = pres->sa.s ;
        char const *name = string + pres->name ;
        char const *state = string + pres->state ;

        if (!ss_state_check(state,name)) log_die(LOG_EXIT_USER,name," : is not initialized") ;
        else if (!ss_state_read(&sta,state,name)) log_dieusys(LOG_EXIT_SYS,"read state file of: ",name) ;

        int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;

        if (obstr_equal(name,SS_MASTER + 1))
        {
            if (pres->ndeps) goto append ;
            else continue ;
        }

        /** logger cannot be unsupervised alone */
        if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc)))
        {
            if (UNSUP) log_die(LOG_EXIT_SYS,"logger detected - unsupervise request is not allowed") ;
        }
        if (UNSUP) unsup = 1 ;
        if (sta.unsupervise) unsup = 1 ;
        append:
        if (pres->type == TYPE_CLASSIC)
        {
            if (unsup)
            {
                if (!ss_resolve_graph_build(&graph_unsup_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
                    log_dieusys(LOG_EXIT_SYS,"build services graph") ;
                if (!ss_resolve_add_logger(&graph_unsup_cl.name,sares.s))
                    log_dieusys(LOG_EXIT_SYS,"append service selection with logger") ;
            }
            if (!ss_resolve_graph_build(&graph_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
                log_dieusys(LOG_EXIT_SYS,"build services graph") ;
            cl++ ;
        }
        else
        {
            if (unsup)
            {
                if (!ss_resolve_graph_build(&graph_unsup_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
                    log_dieusys(LOG_EXIT_SYS,"build services graph") ;
                if (!ss_resolve_add_logger(&graph_unsup_rc.name,sares.s))
                    log_dieusys(LOG_EXIT_SYS,"append service selection with logger") ;
            }
            if (!ss_resolve_graph_build(&graph_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
                log_dieusys(LOG_EXIT_SYS,"build services graph") ;
            rc++;
        }
    }

    /** rc work */
    if (rc)
    {
        log_trace("stop atomic services ...") ;
        if (!rc_down(info,envp))
            log_dieu(LOG_EXIT_SYS,"stop atomic services") ;
        log_trace("switch atomic services of: ",info->treename.s," to source") ;
        if (!db_switch_to(info,envp,SS_SWSRC))
            log_dieu(LOG_EXIT_SYS,"switch",info->livetree.s,"/",info->treename.s," to source") ;

    }

    /** svc work */
    if (cl)
    {
        log_trace("stop classic services ...") ;
        if (!svc_down(info,envp))
            log_dieu(LOG_EXIT_SYS,"stop classic services") ;
        log_trace("switch classic services of: ",info->treename.s," to source") ;
        if (!svc_switch_to(info,SS_SWSRC))
            log_dieu(LOG_EXIT_SYS,"switch classic service of: ",info->treename.s," to source") ;
    }

    if (UNSUP)
    {
        log_trace("send signal -an to scandir: ",info->scandir.s) ;
        if (scandir_send_signal(info->scandir.s,"an") <= 0)
            log_dieu(LOG_EXIT_SYS,"send signal to scandir: ", info->scandir.s) ;
    }
    stralloc_free(&sares) ;
    ss_resolve_free(&res) ;
    genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;

    return 0 ;
}
