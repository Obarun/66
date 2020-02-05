/* 
 * ssexec_disable.c
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
#include <errno.h>
//#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

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

static stralloc workdir = STRALLOC_ZERO ;

static void cleanup(void)
{
	int e = errno ;
	rm_rf(workdir.s) ;
	errno = e ;
}

int svc_remove(genalloc *tostop,ss_resolve_t *res, char const *src,ssexec_t *info)
{
	unsigned int i = 0 ;
	genalloc rdeps = GENALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	ss_resolve_t cp = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	size_t newlen ;
	char *name = res->sa.s + res->name ;
	if (!ss_resolve_copy(&cp,res))
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

	if (!ss_resolve_add_rdeps(&rdeps,&cp,src))
	{
		log_warnusys("resolve recursive dependencies of: ",name) ;
		goto err ;
	}
	ss_resolve_free(&cp) ;
	
	if (!ss_resolve_add_logger(&rdeps,src))
	{
		log_warnusys("resolve logger") ;
		goto err ;
	}

	for (;i < genalloc_len(ss_resolve_t,&rdeps) ; i++)
	{
		ss_resolve_t_ref pres = &genalloc_s(ss_resolve_t,&rdeps)[i] ;
		char *string = pres->sa.s ;
		char *name = string + pres->name ;
		char *state = string + pres->state ;
		dst.len = newlen ;
		if (!stralloc_cats(&dst,name)) goto err ;
		if (!stralloc_0(&dst)) goto err ;
		
		log_trace("delete source service file of: ",name) ;
		if (rm_rf(dst.s) < 0)
		{
			log_warnusys("delete source service file: ",dst.s) ;
			goto err ;
		}
		/** service was not initialized */
		if (!ss_state_check(state,name))
		{
			log_trace("Delete resolve file of: ",name) ;
			ss_resolve_rmfile(src,name) ;
		}
		else
		{
			/** modify the resolve file for 66-stop*/
			pres->disen = 0 ;
			log_trace("Write resolve file of: ",name) ;
			if (!ss_resolve_write(pres,src,name)) 
			{
				log_warnusys("write resolve file of: ",name) ;
				goto err ;
			}
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_TRUE) ;
			log_trace("Write state file of: ",name) ;
			if (!ss_state_write(&sta,state,name))
			{
				log_warnusys("write state file of: ",name) ;
				goto err ;
			}
		}
		if (!ss_resolve_cmp(tostop,name))
			if (!ss_resolve_append(tostop,pres)) goto err ;
	}
	
	genalloc_deepfree(ss_resolve_t,&rdeps,ss_resolve_free) ;
	stralloc_free(&dst) ;
	return 1 ;
	
	err:
		ss_resolve_free(&cp) ;
		genalloc_deepfree(ss_resolve_t,&rdeps,ss_resolve_free) ;
		stralloc_free(&dst) ;
		return 0 ;
}

int ssexec_disable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, logname ;
	unsigned int nlongrun, nclassic, stop ;
	
	
	genalloc tostop = GENALLOC_ZERO ;//ss_resolve_t
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_t_ref pres ;
	
	r = nclassic = nlongrun = stop = logname = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">S", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'S' :	stop = 1 ;	break ;
				default : 	log_usage(usage_disable) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) log_usage(usage_disable) ;

	if (!tree_copy(&workdir,info->tree.s,info->treename.s)) log_dieusys(LOG_EXIT_SYS,"create tmp working directory") ;
	
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(workdir.s,name)) log_die_nclean(LOG_EXIT_USER,&cleanup,name," is not enabled") ;
		if (!ss_resolve_read(&res,workdir.s,name)) log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",name) ;
		if (!ss_resolve_append(&gares,&res)) log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"append services selection with: ",name) ;
	}
	
	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
		char *string = pres->sa.s ;
		char  *name = string + pres->name ;
		logname = 0 ;
		if (obstr_equal(name,SS_MASTER + 1))
			log_die_nclean(LOG_EXIT_USER,&cleanup,"nice try peon") ;
		
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc)))
			log_die_nclean(LOG_EXIT_USER,&cleanup,"logger detected - disabling is not allowed") ;
				
		if (!pres->disen)
		{
			log_info(name,": is already disabled") ;
			ss_resolve_free(&res) ;
			continue ;
		}
		if (!svc_remove(&tostop,pres,workdir.s,info))
			log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"remove",name," directory service") ;
		
		if (res.type == TYPE_CLASSIC) nclassic++ ;
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
		if (!ss_resolve_write_master(info,&graph,workdir.s,1))
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
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
		
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&tostop); i++)
		log_info("Disabled successfully: ",genalloc_s(ss_resolve_t,&tostop)[i].sa.s + genalloc_s(ss_resolve_t,&tostop)[i].name) ;
			
	if (stop && genalloc_len(ss_resolve_t,&tostop))
	{
		int nargc = 3 + genalloc_len(ss_resolve_t,&tostop) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		newargv[m++] = "-u" ;
		
		for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&tostop); i++)
			newargv[m++] = genalloc_s(ss_resolve_t,&tostop)[i].sa.s + genalloc_s(ss_resolve_t,&tostop)[i].name ;
		
		newargv[m++] = 0 ;
		
		if (ssexec_stop(nargc,newargv,envp,info))
		{
			genalloc_deepfree(ss_resolve_t,&tostop,ss_resolve_free) ;
			return 111 ;
		}
	}
	
	genalloc_deepfree(ss_resolve_t,&tostop,ss_resolve_free) ;
		
	return 0 ;		
}
	
