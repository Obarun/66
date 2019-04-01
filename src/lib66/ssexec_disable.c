/* 
 * ssexec_disable.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
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
#include <oblibs/error2.h>
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

static void cleanup(char const *dst)
{
	int e = errno ;
	rm_rf(dst) ;
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
		VERBO1 strerr_warnwu1sys("copy resolve file") ;
		goto err ;
	}
	if (!stralloc_cats(&dst,src)) goto err ;
	if (cp.type == CLASSIC)
	{
		if (!stralloc_cats(&dst,SS_SVC)) goto err ;
	}
	else if (!stralloc_cats(&dst,SS_DB SS_SRC)) retstralloc(0,"remove_sv") ;
	if (!stralloc_cats(&dst,"/")) goto err ;
	newlen = dst.len ;

	if (!ss_resolve_add_rdeps(&rdeps,&cp,src))
	{
		VERBO1 strerr_warnwu2sys("resolve recursive dependencies of: ",name) ;
		goto err ;
	}
	ss_resolve_free(&cp) ;
	
	if (!ss_resolve_add_logger(&rdeps,src))
	{
		VERBO1 strerr_warnwu1sys("resolve logger") ;
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
		
		VERBO2 strerr_warni2x("delete source service file of: ",name) ;
		if (rm_rf(dst.s) < 0)
		{
			VERBO1 strerr_warnwu2sys("delete source service file: ",dst.s) ;
			goto err ;
		}
		/** service was not initialized */
		if (!ss_state_check(state,name))
		{
			VERBO2 strerr_warni2x("Delete resolve file of: ",name) ;
			ss_resolve_rmfile(src,name) ;
		}
		else
		{
			/** modify the resolve file for 66-stop*/
			pres->disen = 0 ;
			VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
			if (!ss_resolve_write(pres,src,name)) 
			{
				VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
				goto err ;
			}
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_TRUE) ;
			VERBO2 strerr_warni2x("Write state file of: ",name) ;
			if (!ss_state_write(&sta,state,name))
			{
				VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
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
	
	stralloc workdir = STRALLOC_ZERO ;
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
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'S' :	stop = 1 ;	break ;
				default : exitusage(usage_disable) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(usage_disable) ;

	if (!tree_copy(&workdir,info->tree.s,info->treename.s)) strerr_diefu1sys(111,"create tmp working directory") ;
	
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(workdir.s,name)) { cleanup(workdir.s) ; strerr_dief2x(110,name," is not enabled") ; }
		if (!ss_resolve_read(&res,workdir.s,name)) { cleanup(workdir.s) ; strerr_diefu2sys(111,"read resolve file of: ",name) ; }
		if (!ss_resolve_append(&gares,&res)) {	cleanup(workdir.s) ; strerr_diefu2sys(111,"append services selection with: ",name) ; }
	}
	
	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
		char *string = pres->sa.s ;
		char  *name = string + pres->name ;
		logname = 0 ;
		if (obstr_equal(name,SS_MASTER + 1))
		{
				cleanup(workdir.s) ;
				strerr_dief1x(110,"nice try peon") ;
		}
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc)))
		{
				cleanup(workdir.s) ;
				strerr_dief1x(110,"logger detected - disabling is not allowed") ;
		}		
		if (!pres->disen)
		{
			VERBO1 strerr_warni2x(name,": is already disabled") ;
			ss_resolve_free(&res) ;
			continue ;
		}
		if (!svc_remove(&tostop,pres,workdir.s,info))
		{
			cleanup(workdir.s) ;
			strerr_diefu3sys(111,"remove",name," directory service") ;
		}
		if (res.type == CLASSIC) nclassic++ ;
		else nlongrun++ ;
		
	}
	
	if (nclassic)
	{
		if (!svc_switch_to(info,SS_SWBACK)) 
		{
			cleanup(workdir.s) ;
			strerr_diefu1sys(111,"switch classic service to backup") ;
		}
		
	}
	
	if (nlongrun)
	{	
		ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
		r = ss_resolve_graph_src(&graph,workdir.s,0,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",info->treename.s) ;
		}
		r= ss_resolve_graph_publish(&graph,0) ;
		if (r <= 0) 
		{
			cleanup(workdir.s) ;
			if (r < 0) strerr_dief1x(110,"cyclic graph detected") ;
			strerr_diefu1sys(111,"publish service graph") ;
		}
		if (!ss_resolve_write_master(info,&graph,workdir.s,1))
		{
			cleanup(workdir.s) ;
			strerr_diefu1sys(111,"update inner bundle") ;
		}
		ss_resolve_graph_free(&graph) ;
		if (!db_compile(workdir.s,info->tree.s, info->treename.s,envp))
		{
			cleanup(workdir.s) ;
			strerr_diefu4x(111,"compile ",workdir.s,"/",info->treename.s) ; 
		}
		/** this is an important part, we call s6-rc-update here */
		if (!db_switch_to(info,envp,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",info->treename.s," to backup") ;
		}
		
	}
	
	if (!tree_copy_tmp(workdir.s,info))
	{
		cleanup(workdir.s) ;
		strerr_diefu4x(111,"copy: ",workdir.s," to: ", info->tree.s) ;
	}
	
	cleanup(workdir.s) ;
		
	stralloc_free(&workdir) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
		
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&tostop); i++)
		VERBO1 strerr_warni2x("Disabled successfully: ",genalloc_s(ss_resolve_t,&tostop)[i].sa.s + genalloc_s(ss_resolve_t,&tostop)[i].name) ;
			
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
	
