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
int svc_remove(genalloc *tostop,ss_resolve_t *res, char const *src)
{
	char *name = res->sa.s + res->name ;
	size_t namelen = strlen(name) ;
	size_t srclen = strlen(src) ; 
	
	char dst[srclen + SS_SVC_LEN + 1 + namelen + 1] ;
	memcpy(dst,src,srclen) ;
	memcpy(dst + srclen, SS_SVC, SS_SVC_LEN) ;
	dst[srclen + SS_SVC_LEN]  =  '/' ;
	memcpy(dst + srclen + SS_SVC_LEN + 1, name, namelen) ;
	dst[srclen + SS_SVC_LEN + 1 + namelen] = 0 ;
		
	VERBO1 strerr_warni3x("Removing: ",name," directory service") ;
	if (rm_rf(dst) < 0)
	{
		VERBO1 strerr_warnwu2sys("remove: ", dst) ;
		return 0 ;
	}
	/** modify the resolve file for 66-stop*/
	res->disen = 0 ;
	res->reload = 0 ;
	res->init = 0 ;
	res->unsupervise = 1 ;
	VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
	if (!ss_resolve_write(res,src,name)) 
	{
		VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
		return 0 ;
	}
	/** if a logger is associated modify the resolve for it */
	if (res->logger) 
	{
		VERBO2 strerr_warni2x("Write logger resolve file of: ",name) ;
		if (!ss_resolve_setlognwrite(res,src))
		{
			VERBO1 strerr_warnwu2sys("write logger resolve file of: ",name) ;
			return 0 ;
		}
		if (!stra_add(tostop,res->sa.s + res->logger)) retstralloc(0,"main") ;
	}
	
	return 1 ;
}

int rc_remove(genalloc *toremove, ss_resolve_t *res, char const *src)
{
	int r ;
	char *name = res->sa.s + res->name ;
	size_t newlen ;
		
	stralloc sa = STRALLOC_ZERO ;
			
	ss_resolve_t dres = RESOLVE_ZERO ;
		
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	
	/** build dependencies graph*/
	r = graph_type_src(&tokeep,src,1) ;
	if (r <= 0)
	{
		VERBO1 strerr_warnwu2x("resolve source of graph for tree: ",src) ;
		goto err ;
	}
	if (!graph_build(&g,&sagraph,&tokeep,src))
	{
		VERBO1 strerr_warnwu1x("make dependencies graph") ;
		goto err ;
	}
	
	r = graph_rdepends(toremove,&g,name,src) ;
	if (!r) 
	{
		VERBO1 strerr_warnwu2x("find services depending for: ",name) ;
		goto err ;
	}
	if(r == 2) VERBO2 strerr_warnt2x("any services depends of: ",name) ;
	
	if (!stra_cmp(toremove,name))
	{
		if (!stra_add(toremove,name)) 
		{	 
			VERBO1 strerr_warnwu3x("add: ",name," as dependency to remove") ;
			goto err ;
		}
	}
	if (!stralloc_cats(&sa,src)) retstralloc(0,"remove_sv") ;
	if (!stralloc_cats(&sa,SS_DB SS_SRC)) retstralloc(0,"remove_sv") ;
	if (!stralloc_cats(&sa, "/")) retstralloc(0,"remove_sv") ;
	newlen = sa.len ;
	genalloc_reverse(stralist,toremove) ;		
	for (unsigned int i = 0; i < genalloc_len(stralist,toremove); i++)
	{
		ss_resolve_init(&dres) ;
		char *dname = gaistr(toremove,i) ;
		sa.len = newlen ;
		if (!stralloc_cats(&sa,dname)) retstralloc(0,"remove_sv") ;
		if (!stralloc_0(&sa)) retstralloc(0,"remove_sv") ;
		VERBO1 strerr_warni3x("Removing: ",dname," directory service") ;
		if (rm_rf(sa.s) < 0)
		{
			VERBO1 strerr_warnwu2sys("remove: ", sa.s) ;
			goto err ;
		}
	
		if (!ss_resolve_read(&dres,src,dname))
		{
			VERBO1 strerr_warnwu2sys("read resolve file of: ",dname) ;
			goto err ;
		}
		dres.disen = 0 ;
		dres.reload = 0 ;
		dres.init = 0 ;
		dres.unsupervise = 1 ;
		VERBO2 strerr_warni2x("Write resolve file of: ",dname) ;
		if (!ss_resolve_write(&dres,src,dname)) 
		{
			VERBO1 strerr_warnwu2sys("write resolve file of: ",dname) ;
			goto err ;
		}
		if (dres.logger) 
		{
			sa.len = newlen ;
			if (!stralloc_cats(&sa,dres.sa.s + dres.logger)) retstralloc(0,"remove_sv") ;
			if (!stralloc_0(&sa)) retstralloc(0,"remove_sv") ;
			VERBO1 strerr_warni3x("Removing: ",dres.sa.s + dres.logger," directory service") ;
			if (rm_rf(sa.s) < 0)
			{
				VERBO1 strerr_warnwu2sys("remove: ", sa.s) ;
				goto err ;
			}
			VERBO2 strerr_warni2x("Write logger resolve file of: ",dname) ;
			if (!ss_resolve_setlognwrite(&dres,src))
			{
				VERBO1 strerr_warnwu2sys("write logger resolve file of: ",dname) ;
				goto err ;
			}
		}
					
	}
		
	graph_free(&g) ;
	stralloc_free(&sagraph) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	stralloc_free(&sa) ;
	ss_resolve_free(&dres) ;
	return 1 ;
	
	err:
		graph_free(&g) ;
		stralloc_free(&sagraph) ;
		genalloc_deepfree(stralist,&tokeep,stra_free) ;
		stralloc_free(&sa) ;
		ss_resolve_free(&dres) ;
		return 0 ;
}

int ssexec_disable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, logname ;
	unsigned int nlongrun, nclassic, stop ;
	
	stralloc workdir = STRALLOC_ZERO ;
	
	genalloc tostop = GENALLOC_ZERO ;//stralist
	
	ss_resolve_t res = RESOLVE_ZERO ;
	
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
	
	{
				
		for(;*argv;argv++)
		{
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
			if (!ss_resolve_check(info,name,SS_RESOLVE_SRC))
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
				strerr_warni2x(name,": is already disabled") ;
				continue ;
			}
			
			if (res.type == CLASSIC)
			{
				if (!svc_remove(&tostop,&res,workdir.s))
				{
					cleanup(workdir.s) ;
					strerr_diefu3sys(111,"remove",name," directory service") ;
				}
				if (!stra_add(&tostop,name))
				{
					cleanup(workdir.s) ;
					retstralloc(111,"main") ;
				}
				nclassic++ ;
			}	
			else if (res.type >= BUNDLE)
			{
				if (!stra_cmp(&tostop,name))
				{
					if (!rc_remove(&tostop,&res,workdir.s))
					{
						cleanup(workdir.s) ;
						strerr_diefu2x(111,"disable: ",name) ;
					}
				}
				nlongrun++ ;
			}
		}
	}
	ss_resolve_free(&res) ;
	
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
		if (!db_write_master(info,&master,workdir.s))
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
	workdir.len = 0 ;
	
	stralloc_free(&workdir) ;
	/** graph allocation */
	graph_free(&g) ;
	stralloc_free(&sagraph) ;	
	genalloc_deepfree(stralist,&master,stra_free) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	if (stop && genalloc_len(stralist,&tostop))
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&tostop) ; i++)
		{
			char *name = gaistr(&tostop,i) ;
			int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
			if (logname > 0) 
				if (!stra_remove(&tostop,name)) strerr_diefu1sys(111,"logger from the list to stop") ;
		}	
		int nargc = 3 + genalloc_len(stralist,&tostop) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		newargv[m++] = "-u" ;
		
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&tostop); i++)
			newargv[m++] = gaistr(&tostop,i) ;
		
		newargv[m++] = 0 ;
		
		if (ssexec_stop(nargc,newargv,envp,info))
		{
			genalloc_deepfree(stralist,&tostop,stra_free) ;
			return 111 ;
		}
	}
	
	genalloc_deepfree(stralist,&tostop,stra_free) ;
		
	return 0 ;		
}
	
