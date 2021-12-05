/*
 * ssexec_disable.c
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
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/resolve.h>
#include <66/svc.h>
#include <66/state.h>
#include <66/utils.h>
#include <66/service.h>

static stralloc workdir = STRALLOC_ZERO ;
static uint8_t FORCE = 0 ;
static uint8_t REMOVE = 0 ;

static void cleanup(void)
{
    log_flow() ;

    int e = errno ;
    rm_rf(workdir.s) ;
    errno = e ;
}

int svc_remove(genalloc *tostop,resolve_service_t *res, char const *src,ssexec_t *info)
{
    log_flow() ;

    unsigned int i = 0 ;
    int r, e = 0 ;
    genalloc rdeps = GENALLOC_ZERO ;
    stralloc dst = STRALLOC_ZERO ;
    resolve_service_t cp = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &cp) ;
    ss_state_t sta = STATE_ZERO ;

    size_t newlen ;
    char *name = res->sa.s + res->name ;
    if (!service_resolve_copy(&cp,res))
    {
        log_warnusys("copy resolve file") ;
        goto err ;
    }
    if (!stralloc_cats(&dst,src)) goto err ;
    if (cp.type == TYPE_CLASSIC)
    {
        if (!stralloc_cats(&dst,SS_SVC)) goto err ;
    }
    else if (!stralloc_cats(&dst,SS_DB SS_SRC)) goto err ;

    if (!stralloc_cats(&dst,"/")) goto err ;

    newlen = dst.len ;

    if (!FORCE)
    {
        if (!service_resolve_add_rdeps(&rdeps,&cp,src))
        {
            log_warnusys("resolve recursive dependencies of: ",name) ;
            goto err ;
        }
    }
    else
    {
        if (!resolve_append(&rdeps,wres)) goto err ;
    }

    if (!service_resolve_add_logger(&rdeps,src))
    {
        log_warnusys("resolve logger") ;
        goto err ;
    }

    resolve_free(wres) ;

    for (;i < genalloc_len(resolve_service_t,&rdeps) ; i++)
    {
        resolve_service_t_ref pres = &genalloc_s(resolve_service_t,&rdeps)[i] ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(SERVICE_STRUCT, pres) ;
        char *str = pres->sa.s ;
        char *name = str + pres->name ;
        char *ste = str + pres->state ;
        dst.len = newlen ;
        if (!stralloc_cats(&dst,name)) goto err ;
        if (!stralloc_0(&dst)) goto err ;

        log_trace("delete source service file of: ",name) ;
        if (rm_rf(dst.s) < 0)
        {
            log_warnusys("delete source service file: ",dst.s) ;
            goto err ;
        }
        /** r == -1 means the state file is not present,
         * r > 0 means service need to be initialized,
         * so not initialized at all.*/
        r = state_check_flags(ste,name,SS_FLAGS_INIT) ;

        if (!r)
        {
            /** modify the resolve file for 66-stop*/
            pres->disen = 0 ;
            log_trace("Write resolve file of: ",name) ;
            if (!resolve_write(dwres,src,name))
            {
                log_warnusys("write resolve file of: ",name) ;
                goto err ;
            }
            if (!state_read(&sta,ste,name)) {
                log_warnusys("read state of: ",name) ;
                goto err ;
            }
            state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
            state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
            state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_TRUE) ;
            log_trace("Write state file of: ",name) ;
            if (!state_write(&sta,ste,name))
            {
                log_warnusys("write state file of: ",name) ;
                goto err ;
            }
        }
        else
        {
           if (REMOVE)
            {
                // remove configuration file
                log_trace("Delete configuration file of: ",name) ;
                if (rm_rf(str + pres->srconf) == -1)
                    log_warnusys("remove configuration file of: ",name) ;

                // remove the logger directory
                log_trace("Delete logger directory of: ",name) ;
                if (rm_rf(str + pres->dstlog) == -1)
                    log_warnusys("remove logger directory of: ",name) ;

                if (pres->type == TYPE_MODULE) {

                    // remove service source file
                    log_trace("Delete service file of: ",name) ;
                    if (rm_rf(str + pres->src) == -1)
                        log_warnusys("remove configuration file of: ",name) ;
                }

            }

            log_trace("Delete resolve file of: ",name) ;
            resolve_rmfile(src,name) ;
        }
        if (!resolve_cmp(tostop, name, SERVICE_STRUCT))
            if (!resolve_append(tostop,wres)) goto err ;

        free(dwres) ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        resolve_deep_free(SERVICE_STRUCT, &rdeps) ;
        stralloc_free(&dst) ;
        return e ;
}

int ssexec_disable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    int r, logname ;
    unsigned int nlongrun, nclassic, stop, force ;

    genalloc tostop = GENALLOC_ZERO ;//resolve_service_t
    genalloc gares = GENALLOC_ZERO ; //resolve_service_t
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_service_t_ref pres ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    r = nclassic = nlongrun = stop = logname = force = 0 ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">" OPTS_DISABLE, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'S' :  if (FORCE) log_usage(usage_disable) ; stop = 1 ; break ;
                case 'F' :  if (stop) log_usage(usage_disable) ; FORCE = 1 ; break ;
                case 'R' :  if (stop) log_usage(usage_disable) ; REMOVE = 1 ; break ;
                default :   log_usage(usage_disable) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(usage_disable) ;

    if (!tree_copy(&workdir,info->tree.s,info->treename.s))
        log_dieusys(LOG_EXIT_SYS,"create tmp working directory") ;

    for (;*argv;argv++)
    {
        char const *name = *argv ;
        if (!resolve_check(workdir.s,name))
            log_info_nclean_return(LOG_EXIT_ZERO,&cleanup,name," is not enabled") ;

        if (!resolve_read(wres,workdir.s,name))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",name) ;

        if (REMOVE)
        {
            stralloc sa = STRALLOC_ZERO ;
            r = service_intree(&sa,name,0) ;
            if (r > 2)
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"use -R option -- ",name," is set on different tree") ;
            stralloc_free(&sa) ;
        }

        if (res.type == TYPE_MODULE)
        {
            if (!module_in_cmdline(&gares,&res,workdir.s))
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"add dependencies of module: ",name) ;
        }
        else
        {
            if (!resolve_append(&gares,wres))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"append services selection with: ",name) ;
        }
    }

    for(unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&gares) ; i++)
    {
        pres = &genalloc_s(resolve_service_t,&gares)[i] ;
        char *string = pres->sa.s ;
        char  *name = string + pres->name ;
        char *state = string + pres->state ;
        uint8_t found = 0 ;
        char module_name[256] ;

        /** The force options can be only used if the service is not marked initialized.
         * This option should only be used when we have a inconsistent state between
         * the /var/lib/66/system/<tree>/servicedirs/ and /var/lib/66/system/<tree>/.resolve
         * directory meaning a service which is not present in the compiled db but its resolve file
         * exist.
         * Also, the remove option can be only used if the service is not marked initialized too.*/
        if (FORCE || REMOVE) {

            r = state_check_flags(state,name,SS_FLAGS_INIT) ;

            if (!r)
                log_die_nclean(LOG_EXIT_USER,&cleanup,name," is marked initialized -- ",FORCE ? "-F" : "-R"," is not allowed") ;
        }

        if (!module_search_service(workdir.s,&gares,name,&found,module_name))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"search in module") ;

        if (found && !FORCE)
            log_die_nclean(LOG_EXIT_USER,&cleanup,name," is a part of: ",module_name," module -- it's not allowed to disable it alone") ;

        if (found && REMOVE)
            log_die_nclean(LOG_EXIT_USER,&cleanup,name," is a part of: ",module_name," module -- -R options is not allowed") ;

        logname = 0 ;
        if (obstr_equal(name,SS_MASTER + 1))
            log_die_nclean(LOG_EXIT_USER,&cleanup,"nice try peon") ;

        logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
        if (logname > 0 && (!resolve_cmp(&gares, string + pres->logassoc, SERVICE_STRUCT)))
            log_die_nclean(LOG_EXIT_USER,&cleanup,"logger detected - disabling is not allowed") ;

        if (!pres->disen && !FORCE)
        {
            log_info(name,": is already disabled") ;
            continue ;
        }
        if (!svc_remove(&tostop,pres,workdir.s,info))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"remove service: ",name) ;

        if (pres->type == TYPE_CLASSIC) nclassic++ ;
        else nlongrun++ ;
    }

    if (nclassic)
    {
        if (!svc_switch_to(info,SS_SWBACK))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"switch classic service to backup") ;
    }

    if (nlongrun)
    {
        ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
        r = ss_resolve_graph_src(&graph,workdir.s,0,1) ;
        if (!r)
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"resolve source of graph for tree: ",info->treename.s) ;

        r= ss_resolve_graph_publish(&graph,0) ;
        if (r <= 0)
        {
            cleanup() ;
            if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected") ;
            log_dieusys(LOG_EXIT_SYS,"publish service graph") ;
        }
        if (!service_resolve_write_master(info,&graph,workdir.s,1))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"update inner bundle") ;

        ss_resolve_graph_free(&graph) ;
        if (!db_compile(workdir.s,info->tree.s, info->treename.s,envp))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"compile ",workdir.s,"/",info->treename.s) ;

        /** this is an important part, we call s6-rc-update here */
        if (!db_switch_to(info,envp,SS_SWBACK))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"switch ",info->treename.s," to backup") ;
    }

    if (!tree_copy_tmp(workdir.s,info))
        log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy: ",workdir.s," to: ", info->tree.s) ;

    cleanup() ;

    stralloc_free(&workdir) ;
    resolve_free(wres) ;
    resolve_deep_free(SERVICE_STRUCT, &gares) ;

    for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&tostop); i++)
        log_info("Disabled successfully: ",genalloc_s(resolve_service_t,&tostop)[i].sa.s + genalloc_s(resolve_service_t,&tostop)[i].name) ;

    if (stop && genalloc_len(resolve_service_t,&tostop))
    {
        int nargc = 3 + genalloc_len(resolve_service_t,&tostop) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "fake_name" ;
        newargv[m++] = "-u" ;

        for (unsigned int i = 0 ; i < genalloc_len(resolve_service_t,&tostop); i++)
            newargv[m++] = genalloc_s(resolve_service_t,&tostop)[i].sa.s + genalloc_s(resolve_service_t,&tostop)[i].name ;

        newargv[m++] = 0 ;

        if (ssexec_stop(nargc,newargv,envp,info))
        {
            resolve_deep_free(SERVICE_STRUCT, &tostop) ;
            return 111 ;
        }
    }

    resolve_deep_free(SERVICE_STRUCT, &tostop) ;

    return 0 ;
}

