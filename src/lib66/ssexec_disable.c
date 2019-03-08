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
#include <sys/stat.h>
#include <stdlib.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/direntry.h>
#include <skalibs/diuint32.h>
 
#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/backup.h>
#include <66/graph.h>
#include <66/resolve.h>
#include <66/svc.h>

#include <stdio.h>

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
	size_t newlen ;
	
	char *name = res->sa.s + res->name ;
	if (!stralloc_cats(&dst,src)) goto err ;
	if (res->type == CLASSIC)
	{
		if (!stralloc_cats(&dst,SS_SVC)) goto err ;
	}
	else if (!stralloc_cats(&dst,SS_DB SS_SRC)) retstralloc(0,"remove_sv") ;
	if (!stralloc_cats(&dst,"/")) goto err ;
	newlen = dst.len ;
	
	if (!ss_resolve_add_rdeps(&rdeps,res,info))
	{
		VERBO1 strerr_warnwu2sys("resolve recursive dependencies of: ",name) ;
		goto err ;
	}
	if (!ss_resolve_add_logger(&rdeps,info))
	{
		VERBO1 strerr_warnwu1sys("resolve logger") ;
		goto err ;
	}
	for (;i < genalloc_len(ss_resolve_t,&rdeps) ; i++)
	{
		ss_resolve_t_ref pres = &genalloc_s(ss_resolve_t,&rdeps)[i] ;
		char *string = pres->sa.s ;
		char *name = string + pres->name ;
		dst.len = newlen ;
		if (!stralloc_cats(&dst,name)) goto err ;
		if (!stralloc_0(&dst)) goto err ;
		
		VERBO2 strerr_warni2x("delete directory service of: ",name) ;
		if (rm_rf(dst.s) < 0)
		{
			VERBO1 strerr_warnwu2sys("delete directory service: ",dst.s) ;
			goto err ;
		}
		/** modify the resolve file for 66-stop*/
		ss_resolve_setflag(pres,SS_FLAGS_DISEN,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(pres,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(pres,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(pres,SS_FLAGS_UNSUPERVISE,SS_FLAGS_TRUE) ;
		
		VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
		if (!ss_resolve_write(pres,src,name,SS_SIMPLE)) 
		{
			VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
			goto err ;
		}
		if (!ss_resolve_cmp(tostop,name))
			if (!genalloc_append(ss_resolve_t,tostop,pres)) goto err ;
	}

	stralloc_free(&dst) ;
	return 1 ;
	
	err:
		stralloc_free(&dst) ;
		return 0 ;
}

int ssexec_disable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, logname ;
	unsigned int nlongrun, nclassic, stop ;
	
	stralloc workdir = STRALLOC_ZERO ;
	
	genalloc tostop = GENALLOC_ZERO ;//ss_resolve_t
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	genalloc master = GENALLOC_ZERO ;
		
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
	
	for(;*argv;argv++)
	{
		ss_resolve_t res = RESOLVE_ZERO ;
		char const *name = *argv ;
		logname = 0 ;
		if (obstr_equal(name,SS_MASTER + 1))
		{
				cleanup(workdir.s) ;
				strerr_dief1x(110,"nice try peon") ;
		}
		ss_resolve_init(&res) ;
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0)
		{
				cleanup(workdir.s) ;
				strerr_dief1x(110,"logger detected - disabling is not allowed") ;
		}
		if (!ss_resolve_check(info,name,SS_RESOLVE_LIVE))
		{
				cleanup(workdir.s) ;
				strerr_dief2x(110,name," is not enabled") ;
		}
		if (!ss_resolve_read(&res,workdir.s,name))
		{
			cleanup(workdir.s) ;
			strerr_diefu2sys(111,"read resolve file of: ",name) ;
		}
		
		if (!res.disen)
		{
			VERBO1 strerr_warni2x(name,": is already disabled") ;
			continue ;
		}
		if (!svc_remove(&tostop,&res,workdir.s,info))
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
		r = graph_type_src(&tokeep,workdir.s,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",info->treename.s) ;
		}
		if (r < 0)
		{
			if (!stra_add(&master,""))
			{
				cleanup(workdir.s) ;
				retstralloc(111,"main") ;
			}
		}
		else 
		{
			if (!graph_build(&g,&sagraph,&tokeep,workdir.s))
			{
				cleanup(workdir.s) ;
				strerr_diefu1x(111,"make dependencies graph") ;
			}
			if (graph_sort(&g) < 0)
			{
				cleanup(workdir.s) ;
				strerr_dief1x(111,"cyclic graph detected") ;
			}
			
			if (!graph_master(&master,&g))
			{
				cleanup(workdir.s) ;
				strerr_dief1x(111,"find master service") ;
			}
		}
		genalloc_reverse(stralist,&master) ;
		if (!db_write_master(info,&master,workdir.s,SS_SIMPLE))
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"update bundle: ", SS_MASTER) ;
		}
			
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
	/** graph allocation */
	graph_free(&g) ;
	stralloc_free(&sagraph) ;	
	genalloc_deepfree(stralist,&master,stra_free) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
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
	
