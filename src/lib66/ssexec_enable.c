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

//#include <stdio.h>

static unsigned int FORCE = 0 ;

static void cleanup(char const *dst)
{
	int e = errno ;
	rm_rf(dst) ;
	errno = e ;
}

static int start_parser(char const *src,char const *svname,char const *tree, unsigned int *nbsv)
{
	stralloc sasv = STRALLOC_ZERO ;
	
	if (!parse_service_before(src,svname,tree,nbsv,&sasv,FORCE)) strerr_dief4x(111,"invalid service file: ",src,"/",svname) ;
	
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
	stralloc workdir = STRALLOC_ZERO ;//working dir directory
	stralloc sasrc = STRALLOC_ZERO ;
	stralloc saresolve = STRALLOC_ZERO ;
	genalloc gasrc = GENALLOC_ZERO ; //type diuint32
	genalloc ganlong = GENALLOC_ZERO ; // type stralist
	genalloc ganclassic = GENALLOC_ZERO ; // name of classic service, type stralist
	
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
	
	//PROG = "66-enable" ;
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
	
	for(;*argv;argv++)
	{
		unsigned int found = 0 ;
		if (!resolve_src(&gasrc,&sasrc,*argv,src,&found))
		{
			src = SS_SERVICE_PACKDIR ;
			if (!resolve_src(&gasrc,&sasrc,*argv,src,&found))
				strerr_diefu2x(111,"resolve source of service file: ",*argv) ;
		}
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(diuint32,&gasrc) ; i++)
		start_parser(sasrc.s + genalloc_s(diuint32,&gasrc)[i].right,sasrc.s + genalloc_s(diuint32,&gasrc)[i].left,info->tree.s,&nbsv) ;
	
	sv_alltype svblob[nbsv] ;
	
	for (int i = 0;i < nbsv;i++)
		svblob[i] =  genalloc_s(sv_alltype,&gasv)[i] ;
	
	sv_db before = {
		.services = svblob, 
		.nsv = nbsv ,
		.string = keep.s ,
		.stringlen = keep.len ,
		.ndeps = genalloc_len(unsigned int,&gadeps) 
	} ;
	
	{
		if (!tree_copy(&workdir,info->tree.s,info->treename)) strerr_diefu1sys(111,"create tmp working directory") ;
		for (unsigned int i = 0; i < before.nsv; i++)
		{
			r = write_services(&before.services[i], workdir.s,FORCE) ;
			if (!r)
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"write service: ",keep.s+before.services[i].cname.name) ;
			}
			if (r > 1) continue ; //service already added
			
			if (before.services[i].cname.itype > CLASSIC)
			{
				if (!stra_add(&ganlong,keep.s + before.services[i].cname.name)) retstralloc(111,"main") ;
				nlongrun++ ;
			}
			else if (before.services[i].cname.itype == CLASSIC) 
			{
				if (!stra_add(&ganclassic,keep.s + before.services[i].cname.name)) retstralloc(111,"main") ;
				nclassic++ ;
			}
		}
		
	}
	
	if (nclassic)
	{
		if (!svc_switch_to(info,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",info->treename," to backup") ;
		}	
	}
	
	if(nlongrun)
	{
		r = graph_type_src(&tokeep,workdir.s,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",info->treename) ;
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
			if (!graph_sort(&g))
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

		if (!db_write_contents(&master,SS_MASTER + 1,workdir.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"update bundle: ", SS_MASTER + 1) ;
		}
				
		if (!db_compile(workdir.s,info->tree.s,info->treename,envp))
		{
				cleanup(workdir.s) ;
				strerr_diefu4x(111,"compile ",workdir.s,"/",info->treename) ;
		}
		
		/** this is an important part, we call s6-rc-update here */
		if (!db_switch_to(info,envp,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",info->treename," to backup") ;
		}		
	}

	if (!tree_copy_tmp(workdir.s,info))
	{
		cleanup(workdir.s) ;
		strerr_diefu4x(111,"copy: ",workdir.s," to: ", info->tree.s) ;
	}
	
	cleanup(workdir.s) ;
	workdir = stralloc_zero ;
	
	for (int i = 0 ; i < genalloc_len(stralist,&ganclassic) ; i++)
	{
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		if (resolve_read(&workdir,saresolve.s,gaistr(&ganclassic,i),"remove")) 
			if (!resolve_remove(saresolve.s,gaistr(&ganclassic,i),"remove"))
				strerr_diefu3sys(111,"delete resolve file",saresolve.s,"/remove") ;
	}
	
	for (int i = 0 ; i < genalloc_len(stralist,&ganlong) ; i++)
	{
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		if (resolve_read(&workdir,saresolve.s,gaistr(&ganlong,i),"remove")) 
			if (!resolve_remove(saresolve.s,gaistr(&ganlong,i),"remove"))
				strerr_diefu3sys(111,"delete resolve file",saresolve.s,"/remove") ;
	}
				
	if (start && ((nclassic) || (nlongrun)))
	{
		int nargc = 2 + genalloc_len(stralist,&ganclassic) + genalloc_len(stralist,&ganlong) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		/** classic */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganclassic); i++)
			newargv[m++] = gaistr(&ganclassic,i) ;
		/** rc */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganlong); i++)
			newargv[m++] = gaistr(&ganlong,i) ;
		
		newargv[m++] = 0 ;
		
		if (ssexec_start(nargc,newargv,envp,info))
			return 111 ;
		
	}
	
	
	/** general allocation*/
	freed_parser() ;
	/** inner allocation */
	stralloc_free(&home) ;
	stralloc_free(&workdir) ;
	stralloc_free(&sasrc) ;
	stralloc_free(&saresolve) ;
	genalloc_free(sv_src_t,&gasrc) ;
	genalloc_deepfree(stralist,&ganclassic,stra_free) ;
	genalloc_deepfree(stralist,&ganlong,stra_free) ;
	
	/** graph stuff */
	genalloc_free(vertex_graph_t,&g.stack) ;
	genalloc_free(vertex_graph_t,&g.vertex) ;
	genalloc_deepfree(stralist,&master,stra_free) ;
	stralloc_free(&sagraph) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	return 0 ;		
}
