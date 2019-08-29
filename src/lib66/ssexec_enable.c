/* 
 * ssexec_enable.c
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
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/diuint32.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/parser.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>

/** force == 1, only rewrite the service
 * force == 2, rewrite the service and it dependencies*/
static uint8_t FORCE = 0 ;
/** rewrite configuration file */
static uint8_t CONF = 0 ;

static void cleanup(char const *dst)
{
	int e = errno ;
	rm_rf(dst) ;
	errno = e ;
}

static void check_identifier(char const *name)
{
	if (!memcmp(name,SS_MASTER+1,6)) strerr_dief3x(111,"service: ",name,": starts with reserved prefix") ;
}

static void start_parser(stralloc *list,ssexec_t *info, unsigned int *nbsv)
{
	uint8_t exist = 0 ;
	stralloc sares = STRALLOC_ZERO ;
	stralloc sasv = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;

	tmp.len = 0 ;
	size_t i = 0, len = list->len ;
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	for (;i < len; i += strlen(list->s + i) + 1)
	{
		exist = 0 ;
		sares.len = 0 ;
		char *name = list->s+i ;
		size_t namelen = strlen(name) ;
		char svname[namelen + 1] ;
		if (!basename(svname,name)) strerr_diefu2sys(111,"get basename of: ", name) ;
		if (ss_resolve_check(sares.s,svname))
		{
			if (!ss_resolve_read(&res,sares.s,svname)) strerr_diefu2sys(111,"read resolve file of: ",svname) ;
			if (res.disen)
			{	
				exist = 1 ;
				if (!FORCE)
				{
					VERBO1 strerr_warnw3x("Ignoring: ",svname," service: already enabled") ;
					continue ;
				}
			}
		}
		
		if (!parse_service_before(info,&tmp,name,nbsv,&sasv,FORCE,&exist))
			strerr_diefu3x(111,"parse service file: ",svname,": or its dependencies") ;
	}
	stralloc_free(&sares) ;
	stralloc_free(&sasv) ;
	stralloc_free(&tmp) ;
	ss_resolve_free(&res) ;
	
}

int ssexec_enable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	FORCE = 0 ;
	CONF = 0 ;
	
	int r ;
	unsigned int nbsv, nlongrun, nclassic, start ;

	stralloc home = STRALLOC_ZERO ;
	stralloc workdir = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	genalloc tostart = GENALLOC_ZERO ; // type stralist
	
	r = nbsv = nclassic = nlongrun = start = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">cCfFS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'f' :	if (FORCE) exitusage(usage_enable) ; 
							FORCE = 1 ; break ;
				case 'F' : 	if (FORCE) exitusage(usage_enable) ; 
							FORCE = 2 ; break ;
				case 'c' :	if (CONF) exitusage(usage_enable) ; CONF = 1 ; break ;
				case 'C' :	if (CONF) exitusage(usage_enable) ; CONF = 2 ; break ;
				case 'S' :	start = 1 ;	break ;
				default : exitusage(usage_enable) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(usage_enable) ;
		
	for(;*argv;argv++)
	{
		check_identifier(*argv) ;
		if (!ss_resolve_src_path(&sasrc,*argv,info)) strerr_diefu2x(111,"resolve source path of: ",*argv) ;
	}

	start_parser(&sasrc,info,&nbsv) ;
	
	if (!tree_copy(&workdir,info->tree.s,info->treename.s)) strerr_diefu1sys(111,"create tmp working directory") ;

	for (unsigned int i = 0; i < genalloc_len(sv_alltype,&gasv); i++)
	{
		sv_alltype_ref sv = &genalloc_s(sv_alltype,&gasv)[i] ;
		char *name = keep.s + sv->cname.name ;
		r = write_services(info,sv, workdir.s,FORCE,CONF) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"write service: ",name) ;
		}
		if (r > 1) continue ; //service already added
		
		VERBO2 strerr_warni2x("write resolve file of: ",name) ;
		if (!ss_resolve_setnwrite(sv,info,workdir.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"write revolve file for: ",name) ;
		}
		VERBO2 strerr_warni2x("Service written successfully: ", name) ;
		if (!stra_cmp(&tostart,name))
		{
			if (sv->cname.itype == CLASSIC) nclassic++ ;
			else nlongrun++ ;
			if (!stra_add(&tostart,name))
			{
				cleanup(workdir.s) ;
				retstralloc(111,"main") ;
			}
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
		ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
		r = ss_resolve_graph_src(&graph,workdir.s,0,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",info->treename.s) ;
		}
		
		r = ss_resolve_graph_publish(&graph,0) ;
		if (r <= 0) 
		{
			cleanup(workdir.s) ;
			if (r < 0) strerr_dief1x(110,"cyclic graph detected") ;
			strerr_diefu1sys(111,"publish service graph") ;
		}
		if (!ss_resolve_write_master(info,&graph,workdir.s,0))
		{
			cleanup(workdir.s) ;
			strerr_diefu1sys(111,"update inner bundle") ;
		}
		ss_resolve_graph_free(&graph) ;
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

	/** parser allocation*/
	freed_parser() ;
	/** inner allocation */
	stralloc_free(&home) ;
	stralloc_free(&workdir) ;
	stralloc_free(&sasrc) ;
		
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tostart); i++)
		VERBO1 strerr_warni2x("Enabled successfully: ", gaistr(&tostart,i)) ;
	
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
