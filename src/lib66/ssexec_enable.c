/*
 * ssexec_enable.c
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
#include <stdint.h>
#include <errno.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/parser.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/environ.h>

static stralloc workdir = STRALLOC_ZERO ;
/** force == 1, only rewrite the service
 * force == 2, rewrite the service and it dependencies*/
static uint8_t FORCE = 0 ;
/** rewrite configuration file */
static uint8_t CONF = 0 ;

void ssexec_enable_cleanup(void)
{
    log_flow() ;

    if (!workdir.len)
        return ;
    int e = errno ;
    rm_rf(workdir.s) ;
    errno = e ;
}

static void check_identifier(char const *name)
{
    log_flow() ;

    int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
    if (logname > 0) log_die(LOG_EXIT_USER,"service: ",name,": ends with reserved suffix -log") ;
    if (!memcmp(name,SS_MASTER+1,6)) log_die(LOG_EXIT_USER,"service: ",name,": starts with reserved prefix Master") ;
    if (!strcmp(name,SS_SERVICE)) log_die(LOG_EXIT_USER,"service as service name is a reserved name") ;
    if (!strcmp(name,"service@")) log_die(LOG_EXIT_USER,"service@ as service name is a reserved name") ;
}

void start_parser(stralloc *list,ssexec_t *info, unsigned int *nbsv,uint8_t force)
{
    log_flow() ;

    size_t i = 0 ;

    stralloc sasv = STRALLOC_ZERO ;
    stralloc parsed_list = STRALLOC_ZERO ;
    stralloc tree_list = STRALLOC_ZERO ;
    uint8_t disable_module = 1 ;

    FOREACH_SASTR(list,i) {

        char *name = list->s + i ;

        if (!parse_service_before(info,&parsed_list,&tree_list,name,nbsv,&sasv,force,CONF,disable_module,0))
            log_dieu(LOG_EXIT_SYS,"parse service file: ",name,": or its dependencies") ;
    }
    stralloc_free(&sasv) ;
    stralloc_free(&parsed_list) ;
    stralloc_free(&tree_list) ;
}

void start_write(stralloc *tostart,unsigned int *nclassic,unsigned int *nlongrun,char const *workdir, genalloc *gasv,ssexec_t *info,uint8_t FORCE,uint8_t CONF)
{
    log_flow() ;

    int r ;
    stralloc module = STRALLOC_ZERO ;
    stralloc version = STRALLOC_ZERO ;

    for (unsigned int i = 0; i < genalloc_len(sv_alltype,gasv); i++)
    {
        sv_alltype_ref sv = &genalloc_s(sv_alltype,gasv)[i] ;
        char *name ;

        r = write_services(sv, workdir,FORCE,CONF) ;
        if (!r)
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"write service: ",name) ;

        if (r > 1) continue ; //service already added

        /** only read name after the write_services process.
         * it change the sv_alltype appending the real_exec element */
        name = keep.s + sv->cname.name ;

        log_trace("write resolve file of: ",name) ;
        if (!ss_resolve_setnwrite(sv,info,workdir))
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"write revolve file for: ",name) ;

        if (sastr_cmp(tostart,name) == -1)
        {
            if (sv->cname.itype == TYPE_CLASSIC) (*nclassic)++ ;
            else (*nlongrun)++ ;
            if (!sastr_add_string(tostart,name))
                log_dieusys_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"stralloc") ;
        }

        log_trace("Service written successfully: ", name) ;

        if (sv->cname.itype == TYPE_MODULE)
            stralloc_catb(&module,name,strlen(name) + 1) ;
    }

    if (module.len)
    {
        /** we need to rewrite the contents file of each module
         * in the good order. we cannot make this before here because
         * we need the resolve file for each services*/
        int r ;
        size_t pos = 0, gpos = 0 ;
        size_t workdirlen = strlen(workdir) ;
        ss_resolve_t res = RESOLVE_ZERO ;
        ss_resolve_t dres = RESOLVE_ZERO ;
        stralloc salist = STRALLOC_ZERO ;
        genalloc gamodule = GENALLOC_ZERO ;
        ss_resolve_graph_t mgraph = RESOLVE_GRAPH_ZERO ;
        char *name = 0 ;
        char *err_msg = 0 ;

        FOREACH_SASTR(&module, pos) {

            salist.len = 0 ;
            name = module.s + pos ;
            size_t namelen = strlen(name) ;
            char dst[workdirlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen + 1];
            auto_strings(dst,workdir,SS_DB,SS_SRC,"/",name) ;

            if (!ss_resolve_read(&res,workdir,name)) {
                err_msg = "read resolve file of: " ;
                goto err ;
            }

            if (!sastr_clean_string(&salist,res.sa.s + res.contents)) {
                err_msg = "rebuild dependencies list of: " ;
                goto err ;
            }

            {
                gpos = 0 ;
                FOREACH_SASTR(&salist,gpos) {

                    if (!ss_resolve_read(&dres,workdir,salist.s + gpos)) {
                        err_msg = "read resolve file of: " ;
                        goto err ;
                    }

                    if (dres.type != TYPE_CLASSIC)
                    {
                        if (ss_resolve_search(&gamodule,name) == -1)
                        {
                            if (!ss_resolve_append(&gamodule,&dres))
                            {
                                err_msg = "append genalloc with: " ;
                                goto err ;
                            }
                        }
                    }
                }
            }

            for (gpos = 0 ; gpos < genalloc_len(ss_resolve_t,&gamodule) ; gpos++)
            {
                if (!ss_resolve_graph_build(&mgraph,&genalloc_s(ss_resolve_t,&gamodule)[gpos],workdir,0))
                {
                    err_msg = "build the graph of: " ;
                    goto err ;
                }
            }

            r = ss_resolve_graph_publish(&mgraph,0) ;
            if (r < 0) {
                err_msg = "publish graph -- cyclic graph detected in: " ;
                goto err ;
            }
            else if (!r)
            {
                err_msg = "publish service graph of: " ;
                goto err ;
            }

            salist.len = 0 ;
            for (gpos = 0 ; gpos < genalloc_len(ss_resolve_t,&mgraph.sorted) ; gpos++)
            {
                char *string = genalloc_s(ss_resolve_t,&mgraph.sorted)[gpos].sa.s ;
                char *name = string + genalloc_s(ss_resolve_t,&mgraph.sorted)[gpos].name ;
                if (!auto_stra(&salist,name,"\n"))
                {
                    err_msg = "append stralloc for: " ;
                    goto err ;
                }
            }

            /** finally write it */
            if (!file_write_unsafe(dst,SS_CONTENTS,salist.s,salist.len))
            {
                err_msg = "create contents file of: "  ;
                goto err ;
            }

            genalloc_deepfree(ss_resolve_t,&gamodule,ss_resolve_free) ;
            ss_resolve_graph_free(&mgraph) ;
        }

        stralloc_free(&module) ;
        stralloc_free(&version) ;
        return ;

        err:
            genalloc_deepfree(ss_resolve_t,&gamodule,ss_resolve_free) ;
            ss_resolve_graph_free(&mgraph) ;
            ss_resolve_free(&res) ;
            ss_resolve_free(&dres) ;
            stralloc_free(&salist) ;
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,err_msg,name) ;
    }
    stralloc_free(&module) ;
    stralloc_free(&version) ;
}

int ssexec_enable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    // be sure that the global var are set correctly
    FORCE = CONF = 0 ;

    int r ;
    size_t pos = 0 ;
    unsigned int nbsv, nlongrun, nclassic, start ;

    stralloc sasrc = STRALLOC_ZERO ;
    stralloc tostart = STRALLOC_ZERO ;

    r = nbsv = nclassic = nlongrun = start = 0 ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">" OPTS_ENABLE, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'f' :  if (FORCE) log_usage(usage_enable) ;
                            FORCE = 1 ; break ;
                case 'F' :  if (FORCE) log_usage(usage_enable) ;
                            FORCE = 2 ; break ;
                case 'I' :  CONF = 1 ; break ;
                case 'S' :  start = 1 ; break ;
                default :   log_usage(usage_enable) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(usage_enable) ;

    for(;*argv;argv++)
    {
        check_identifier(*argv) ;
        size_t len = strlen(*argv) ;
        char const *sv = 0 ;
        char const *directory_forced = 0 ;
        char bname[len + 1] ;
        char dname[len + 1] ;

        if (argv[0][0] == '/') {
            if (!ob_dirname(dname,*argv))
                log_dieu(LOG_EXIT_SYS,"get dirname of: ",*argv) ;
            if (!ob_basename(bname,*argv)) log_dieu(LOG_EXIT_SYS,"get basename of: ",*argv) ;
            sv = bname ;
            directory_forced = dname ;
        } else  sv = *argv ;

        if (ss_resolve_src_path(&sasrc,sv,info->owner,!directory_forced ? 0 : directory_forced) < 1) log_dieu(LOG_EXIT_SYS,"resolve source path of: ",*argv) ;
    }

    start_parser(&sasrc,info,&nbsv,FORCE) ;

    if (!tree_copy(&workdir,info->tree.s,info->treename.s)) log_dieusys(LOG_EXIT_SYS,"create tmp working directory") ;

    start_write(&tostart,&nclassic,&nlongrun,workdir.s,&gasv,info,FORCE,CONF) ;

    if (nclassic)
    {
        if (!svc_switch_to(info,SS_SWBACK))
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"switch ",info->treename.s," to backup") ;
    }

    if(nlongrun)
    {
        ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
        r = ss_resolve_graph_src(&graph,workdir.s,0,1) ;
        if (!r)
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"resolve source of graph for tree: ",info->treename.s) ;

        r = ss_resolve_graph_publish(&graph,0) ;
        if (r <= 0)
        {
            ssexec_enable_cleanup() ;
            if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected") ;
            log_dieusys(LOG_EXIT_SYS,"publish service graph") ;
        }
        if (!ss_resolve_write_master(info,&graph,workdir.s,0))
            log_dieusys_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"update inner bundle") ;

        ss_resolve_graph_free(&graph) ;
        if (!db_compile(workdir.s,info->tree.s,info->treename.s,envp))
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"compile ",workdir.s,"/",info->treename.s) ;

        /** this is an important part, we call s6-rc-update here */
        if (!db_switch_to(info,envp,SS_SWBACK))
            log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"switch ",info->treename.s," to backup") ;
    }

    if (!tree_copy_tmp(workdir.s,info))
        log_dieu_nclean(LOG_EXIT_SYS,&ssexec_enable_cleanup,"copy: ",workdir.s," to: ", info->tree.s) ;


    ssexec_enable_cleanup() ;

    /** parser allocation*/
    freed_parser() ;
    /** inner allocation */
    stralloc_free(&workdir) ;
    stralloc_free(&sasrc) ;

    for (; pos < tostart.len; pos += strlen(tostart.s + pos) + 1)
        log_info("Enabled successfully: ", tostart.s + pos) ;

    if (start && tostart.len)
    {
        int nargc = 2 + sastr_len(&tostart) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "fake_name" ;

        for (pos = 0 ; pos < tostart.len; pos += strlen(tostart.s + pos) + 1)
            newargv[m++] = tostart.s + pos ;

        newargv[m++] = 0 ;

        if (ssexec_start(nargc,newargv,envp,info))
        {
            stralloc_free(&tostart) ;
            return 111 ;
        }
    }

    stralloc_free(&tostart) ;

    return 0 ;
}
