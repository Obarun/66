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
//#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

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

static stralloc workdir = STRALLOC_ZERO ;
/** force == 1, only rewrite the service
 * force == 2, rewrite the service and it dependencies*/
static uint8_t FORCE = 0 ;
/** rewrite configuration file */
static uint8_t CONF = 0 ;

static void cleanup(void)
{
	int e = errno ;
	rm_rf(workdir.s) ;
	errno = e ;
}

static void check_identifier(char const *name)
{
	int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
	if (logname > 0) log_die(LOG_EXIT_USER,"service: ",name,": ends with reserved suffix -log") ;
	if (!memcmp(name,SS_MASTER+1,6)) log_die(LOG_EXIT_USER,"service: ",name,": starts with reserved prefix Master") ;
}

void start_parser(stralloc *list,ssexec_t *info, unsigned int *nbsv,uint8_t FORCE)
{
	int r ;
	uint8_t exist = 0 ;
	size_t i = 0, len = list->len ;
	
	stralloc sasv = STRALLOC_ZERO ;
	stralloc parsed_list = STRALLOC_ZERO ;
	stralloc tree_list = STRALLOC_ZERO ;
	
	for (;i < len; i += strlen(list->s + i) + 1)
	{
		exist = 0 ;
		char *name = list->s+i ;
		size_t namelen = strlen(name) ;
		char svname[namelen + 1] ;
		if (!basename(svname,name)) log_dieusys(LOG_EXIT_SYS,"get basename of: ", svname) ;
		r = parse_service_check_enabled(info,svname,FORCE,&exist) ;
		if (!r) log_dieu(LOG_EXIT_SYS,"check enabled service: ",svname) ;
		if (r == 2) continue ;
		if (!parse_service_before(info,&parsed_list,&tree_list,name,nbsv,&sasv,FORCE,&exist))
			log_dieu(LOG_EXIT_SYS,"parse service file: ",svname,": or its dependencies") ;
	}
	stralloc_free(&sasv) ;
	stralloc_free(&parsed_list) ;
	stralloc_free(&tree_list) ;
}

void start_write(stralloc *tostart,unsigned int *nclassic,unsigned int *nlongrun,char const *workdir, genalloc *gasv,ssexec_t *info,uint8_t FORCE,uint8_t CONF)
{
	int r ;

	for (unsigned int i = 0; i < genalloc_len(sv_alltype,gasv); i++)
	{
		sv_alltype_ref sv = &genalloc_s(sv_alltype,gasv)[i] ;
		char *name = keep.s + sv->cname.name ;
		r = write_services(info,sv, workdir,FORCE,CONF) ;
		if (!r)
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"write service: ",name) ;

		if (r > 1) continue ; //service already added

		log_trace("write resolve file of: ",name) ;
		if (!ss_resolve_setnwrite(sv,info,workdir))
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"write revolve file for: ",name) ;

		log_trace("Service written successfully: ", name) ;
		if (sastr_cmp(tostart,name) == -1)
		{
			if (sv->cname.itype == TYPE_CLASSIC) (*nclassic)++ ;
			else (*nlongrun)++ ;
			if (!sastr_add_string(tostart,name))
				log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"stralloc") ;
		}
	}
}

int ssexec_enable(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	FORCE = 0 ;
	CONF = 0 ;
	
	int r ;
	size_t pos = 0 ;
	unsigned int nbsv, nlongrun, nclassic, start ;
	
	//stralloc home = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	stralloc tostart = STRALLOC_ZERO ;
	
	r = nbsv = nclassic = nlongrun = start = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">cCfFS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'f' :	if (FORCE) log_usage(usage_enable) ; 
							FORCE = 1 ; break ;
				case 'F' : 	if (FORCE) log_usage(usage_enable) ; 
							FORCE = 2 ; break ;
				case 'c' :	if (CONF) log_usage(usage_enable) ; CONF = 1 ; break ;
				case 'C' :	if (CONF) log_usage(usage_enable) ; CONF = 2 ; break ;
				case 'S' :	start = 1 ;	break ;
				default : 	log_usage(usage_enable) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) log_usage(usage_enable) ;
	
	for(;*argv;argv++)
	{
		check_identifier(*argv) ;
		if (ss_resolve_src_path(&sasrc,*argv,info) < 1) log_dieu(LOG_EXIT_SYS,"resolve source path of: ",*argv) ;
	}

	start_parser(&sasrc,info,&nbsv,FORCE) ;
	
	if (!tree_copy(&workdir,info->tree.s,info->treename.s)) log_dieusys(LOG_EXIT_SYS,"create tmp working directory") ;
	
	start_write(&tostart,&nclassic,&nlongrun,workdir.s,&gasv,info,FORCE,CONF) ;

	if (nclassic)
	{
		if (!svc_switch_to(info,SS_SWBACK))
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"switch ",info->treename.s," to backup") ;
	}
	
	if(nlongrun)
	{
		ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
		r = ss_resolve_graph_src(&graph,workdir.s,0,1) ;
		if (!r)
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"resolve source of graph for tree: ",info->treename.s) ;
		
		r = ss_resolve_graph_publish(&graph,0) ;
		if (r <= 0) 
		{
			cleanup() ;
			if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected") ;
			log_dieusys(LOG_EXIT_SYS,"publish service graph") ;
		}
		if (!ss_resolve_write_master(info,&graph,workdir.s,0))
			log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"update inner bundle") ;
		
		ss_resolve_graph_free(&graph) ;
		if (!db_compile(workdir.s,info->tree.s,info->treename.s,envp))
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"compile ",workdir.s,"/",info->treename.s) ;
		
		/** this is an important part, we call s6-rc-update here */
		if (!db_switch_to(info,envp,SS_SWBACK))
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"switch ",info->treename.s," to backup") ;
	}

	if (!tree_copy_tmp(workdir.s,info))
		log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy: ",workdir.s," to: ", info->tree.s) ;
	
	
	cleanup() ;

	/** parser allocation*/
	freed_parser() ;
	/** inner allocation */
	//stralloc_free(&home) ;
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
