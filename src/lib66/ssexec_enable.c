/* 
 * ssexec_enable.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
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
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/diuint32.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/parser.h>
#include <66/backup.h>
#include <66/svc.h>
#include <66/graph.h>
#include <66/resolve.h>
#include <66/ssexec.h>

//#include <stdio.h>

static unsigned int FORCE = 0 ;

static void cleanup(char const *dst)
{
	int e = errno ;
	//rm_rf(dst) ;
	errno = e ;
}

static int start_parser(char const *src,char const *svname,char const *tree, unsigned int *nbsv)
{
	stralloc sasv = STRALLOC_ZERO ;
	
	if (!parse_service_before(src,svname,tree,nbsv,&sasv,FORCE))
		strerr_dief3x(111,"invalid service file: ",src,svname) ;
	
	stralloc_free(&sasv) ;
	
	return 1 ;
}

int ssexec_enable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	FORCE = 0 ;
	
	int r ;
	unsigned int nbsv, nlongrun, nclassic, start ;
	
	char const *src ;
	
	stralloc home = STRALLOC_ZERO ;
	stralloc workdir = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	genalloc gasrc = GENALLOC_ZERO ; //type diuint32
	genalloc tostart = GENALLOC_ZERO ; // type stralist
	
	ss_resolve_t res = RESOLVE_ZERO ;
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc master = GENALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	
	r = nbsv = nclassic = nlongrun = start = 0 ;
	
	if (!info->owner) src = SS_SERVICE_SYSDIR ;
	else
	{	
		if (!set_ownerhome(&home,info->owner)) strerr_diefu1sys(111,"set home directory");
		if (!stralloc_cats(&home,SS_SERVICE_USERDIR)) retstralloc(111,"main") ;
		if (!stralloc_0(&home)) retstralloc(111,"main") ;
		home.len-- ;
		src = home.s ;
	}
		
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">fS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'f' : 	FORCE = 1 ; break ;
				case 'S' :	start = 1 ;	break ;
				default : exitusage(usage_enable) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(usage_enable) ;
	
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	for(;*argv;argv++)
	{
		ss_resolve_t_ref pres = &res ;
		if (ss_resolve_check(info,*argv,SS_RESOLVE_SRC))
		{
			if (!ss_resolve_read(pres,sares.s,*argv)) strerr_diefu2sys(111,"read resolve file of: ",*argv) ;
			if (res.disen && !FORCE)
			{
				VERBO1 strerr_warnw3x("ignoring: ",*argv," service: already enabled") ;
				ss_resolve_free(pres) ;
				continue ;
			}
		}
		unsigned int found = 0 ;
		if (!ss_resolve_src(&gasrc,&sasrc,*argv,src,&found))
		{
			src = SS_SERVICE_PACKDIR ;
			if (!ss_resolve_src(&gasrc,&sasrc,*argv,src,&found))
				strerr_dief2sys(110,"unknow service: ",*argv) ;
		}
		
	}
	ss_resolve_free(&res) ;
	stralloc_free(&sares) ;
	if (!genalloc_len(diuint32,&gasrc)) goto freed ;
	
	for (unsigned int i = 0 ; i < genalloc_len(diuint32,&gasrc) ; i++)
		start_parser(sasrc.s + genalloc_s(diuint32,&gasrc)[i].right,sasrc.s + genalloc_s(diuint32,&gasrc)[i].left,info->tree.s,&nbsv) ;
		
	{
		
		if (!tree_copy(&workdir,info->tree.s,info->treename.s)) strerr_diefu1sys(111,"create tmp working directory") ;
		
		for (unsigned int i = 0; i < genalloc_len(sv_alltype,&gasv); i++)
		{
			ss_resolve_t res = RESOLVE_ZERO ;
			ss_resolve_init(&res) ;
			sv_alltype_ref sv = &genalloc_s(sv_alltype,&gasv)[i] ;
			r = write_services(sv, workdir.s,FORCE) ;
			if (!r)
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"write service: ",keep.s+sv->cname.name) ;
			}
			if (r > 1) continue ; //service already added
			
			if (sv->cname.itype > CLASSIC)
			{
				if (!stra_add(&tostart,keep.s + sv->cname.name))
				{
					cleanup(workdir.s) ;
					retstralloc(111,"main") ;
				}
				nlongrun++ ;
			}
			else if (sv->cname.itype == CLASSIC) 
			{
				if (!stra_add(&tostart,keep.s + sv->cname.name))
				{
					cleanup(workdir.s) ;
					retstralloc(111,"main") ;
				}
				nclassic++ ;
			}
			VERBO2 strerr_warni2x("write resolve file of: ",keep.s + sv->cname.name) ;
			if (!ss_resolve_setnwrite(&res,sv,info,workdir.s))
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"write revolve file for: ",keep.s + sv->cname.name) ;
			}
			ss_resolve_free(&res) ;
		}
		
	}
	

	if (nclassic)
	{
		if (!svc_switch_to(info,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",info->treename.s," to backup") ;
		}	
	}
	
	if(nlongrun)
	{
		r = graph_type_src(&tokeep,workdir.s,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",info->treename.s) ;
		}
		if (r < 0)
		{
			if (!stra_add(&master,"")) retstralloc(111,"main") ;
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
			strerr_diefu2sys(111,"update bundle: ", SS_MASTER + 1) ;
		}
				
		if (!db_compile(workdir.s,info->tree.s,info->treename.s,envp))
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
	
	freed:
	/** parser allocation*/
	freed_parser() ;
	/** inner allocation */
	stralloc_free(&home) ;
	stralloc_free(&workdir) ;
	stralloc_free(&sasrc) ;
	genalloc_free(diuint32,&gasrc) ;
	/** graph allocation */
	graph_free(&g) ;
	genalloc_deepfree(stralist,&master,stra_free) ;
	stralloc_free(&sagraph) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
			
	if (start && genalloc_len(stralist,&tostart))
	{
		int nargc = 2 + genalloc_len(stralist,&tostart) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&tostart); i++)
			newargv[m++] = gaistr(&tostart,i) ;
		
		newargv[m++] = 0 ;
		
		if (ssexec_start(nargc,newargv,envp,info))
		{
			genalloc_deepfree(stralist,&tostart,stra_free) ;
			return 111 ;
		}
		
	}
	
	genalloc_deepfree(stralist,&tostart,stra_free) ;
		
	return 0 ;
}
