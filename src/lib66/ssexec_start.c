/* 
 * ssexec_start.c
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
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/db.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/backup.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/rc.h>

#include <s6/s6-supervise.h>

static int empty = 0 ;
static unsigned int RELOAD = 0 ;
static unsigned int DEADLINE = 0 ;
static char *SIG = "-U" ;

static genalloc nclassic = GENALLOC_ZERO ; //resolve_t type
static genalloc nrc = GENALLOC_ZERO ; //resolve_t type

int svc_sanitize(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	unsigned int i = 0 , reverse = 0 ;
	int r ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_graph_t graph_init = RESOLVE_GRAPH_ZERO ;
	ss_resolve_graph_t graph_reload = RESOLVE_GRAPH_ZERO ;
	
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_LIVE))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to live") ;
		goto err;
	}
	
	for (; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		unsigned int ireverse = reverse ;
		if (genalloc_s(ss_resolve_t,ga)[i].reload) 
		{	
			ireverse = 1 ;
			if (!ss_resolve_graph_build(&graph_reload,&genalloc_s(ss_resolve_t,ga)[i],sares.s,ireverse)) goto err ;
		}
		else if (genalloc_s(ss_resolve_t,ga)[i].init)
		{
			int logname = get_rstrlen_until(genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].name,SS_LOG_SUFFIX) ;
			/** if only the logger was asked to init the main service
			 * will be also initialized, take the logassoc name in this case
			 * to prevent both resolve files*/
			if (logname > 0 && (!ss_resolve_cmp(ga,genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].logassoc)))
			{
				ireverse = 1 ;
			}
			if (!ss_resolve_graph_build(&graph_init,&genalloc_s(ss_resolve_t,ga)[i],sares.s,ireverse)) goto err ;
		}
	}
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_SRC))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
		goto err;
	}
	if (genalloc_len(ss_resolve_t,&graph_reload.name))
	{	
		r = ss_resolve_graph_publish(&graph_reload,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_unsupervise(info,&graph_reload.sorted,"-D",envp))	goto err ;
		genalloc_reverse(ss_resolve_t,&graph_reload.sorted) ;
		if (!svc_init(info,sares.s,&graph_reload.sorted))
		{
			VERBO1 strerr_warnwu1x("iniatiate service list") ;
			goto err ;
		}
		goto end ;
	}
	if (genalloc_len(ss_resolve_t,&graph_init.name))
	{
		r = ss_resolve_graph_publish(&graph_init,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_init(info,sares.s,&graph_init.sorted))
		{
			VERBO1 strerr_warnwu1x("iniatiate service list") ;
			goto err ;
		}
	}
	
	end:
	stralloc_free(&sares) ;
	ss_resolve_graph_free(&graph_reload) ;
	ss_resolve_graph_free(&graph_init) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		ss_resolve_graph_free(&graph_reload) ;
		ss_resolve_graph_free(&graph_init) ;
		return 0 ;
}

int rc_sanitize(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	int r, reverse = 1 ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_graph_t graph_reload = RESOLVE_GRAPH_ZERO ;
	
	char db[info->livetree.len + 1 + info->treename.len + 1] ;
	memcpy(db,info->livetree.s,info->livetree.len) ;
	db[info->livetree.len] = '/' ;
	memcpy(db + info->livetree.len + 1, info->treename.s, info->treename.len) ;
	db[info->livetree.len + 1 + info->treename.len] = 0 ;
	
	if (!db_ok(info->livetree.s,info->treename.s))
	{
		r = rc_init(info,envp) ;
		if (!r) goto err ;
		else if (r > 1) { empty = 1 ; goto end ; }
	}
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_LIVE))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to live") ;
		goto err;
	}
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		if (genalloc_s(ss_resolve_t,ga)[i].reload)
		{
			if (!ss_resolve_graph_build(&graph_reload,&genalloc_s(ss_resolve_t,ga)[i],sares.s,reverse)) goto err ;
		}
	}

	if (genalloc_len(ss_resolve_t,&graph_reload.name))
	{	
		r = ss_resolve_graph_publish(&graph_reload,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!db_switch_to(info,envp,SS_SWBACK))
		{
			VERBO1 strerr_warnwu3x("switch ",info->treename.s," to backup") ;
			return 0 ;
		}
		
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))		
		{
			VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
			goto err ;
		}
		if (!db_compile(sares.s,info->tree.s, info->treename.s,envp))
		{
			VERBO1 strerr_diefu4x(111,"compile ",sares.s,"/",info->treename.s) ;
			goto err ;
		}
		
		if (!db_switch_to(info,envp,SS_SWSRC))
		{
			VERBO1 strerr_warnwu3x("switch ",info->treename.s," to source") ;
			goto err ;
		}
	
		if (!rc_send(info,&graph_reload.sorted,"-d",envp)) goto err ;
	}
	end:
	ss_resolve_graph_free(&graph_reload) ;
	stralloc_free(&sares) ;
	return 1 ;

	err:
		ss_resolve_graph_free(&graph_reload) ;
		stralloc_free(&sares) ;
		return 0 ;
}

int rc_start(ssexec_t *info,genalloc *ga,char const *signal,char const *const *envp)
{
	char const *sig ;
	if (obstr_equal("-U",signal)) sig = "-u" ;
	else sig = signal ;
	
	int r = db_find_compiled_state(info->livetree.s,info->treename.s) ;
	if (r)
	{
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu3x(111,"switch: ",info->treename.s," to source") ;
	}
	if (!rc_send(info,ga,sig,envp)) return 0 ;
	
	return 1 ;
}

int ssexec_start(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	RELOAD = 0 ;
	DEADLINE = 0 ;
	SIG = "-U" ;
		
	if (info->timeout) DEADLINE = info->timeout ;
	
	int cl, rc, logname ;
	stralloc sares = STRALLOC_ZERO ;
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t	
	ss_resolve_t_ref pres ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	cl = rc = logname = 0  ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">rR", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'r' : 	if (RELOAD) exitusage(usage_start) ; RELOAD = 1 ; SIG = "-r" ; break ;
				case 'R' : 	if (RELOAD) exitusage(usage_start) ; RELOAD = 2 ; SIG = "-U" ; break ;
				default : exitusage(usage_start) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) exitusage(usage_start) ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) strerr_diefu1sys(111,"set revolve pointer to live") ;
	/** the tree may not initialized already, check it and create
	 * the live directory if it's the case */
	if (!scan_mode(sares.s,S_IFDIR))
		if (!ss_resolve_create_live(info)) strerr_diefu1sys(111,"create live state") ;
	
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(sares.s,name)) strerr_dief2x(110,name," is not enabled") ;
		if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!ss_resolve_append(&gares,&res)) strerr_diefu2sys(111,"append services selection with: ",name) ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
		char *string = pres->sa.s ;
		char *name = string + pres->name ;
		logname = 0 ;
		size_t earlen = strlen(string + pres->runat) ;
		char earlier[earlen + 8 + 1] ;
		memcpy(earlier,string + pres->runat,earlen) ;
		memcpy(earlier + earlen,"/earlier",8) ;
		earlier[earlen + 8] = 0 ;
		if (!access(earlier, F_OK))
		{ 
			pres->disen = 1 ;
			pres->run = 1 ;
			unlink_void(earlier) ;
		}
		if (obstr_equal(name,SS_MASTER + 1)) goto append ;
		/** always check if the daemon is present or not into the scandir
		 * it can be stopped from different manner (crash,66-scandir signal,..)
		 * without changing the corresponding resolve file */
		if (pres->type == LONGRUN || pres->type == CLASSIC)
		{
			if (!s6_svc_ok(string + pres->runat))
			{
				pres->init = 1 ;
				pres->run = 0 ;
			}
			else 
			{
				pres->init = 0 ;
				pres->run = 1 ;
			}
		}
		if (!pres->disen && pres->run){ VERBO1 strerr_dief3x(111,"service: ",name," was disabled, you can only stop it") ; }
		else if (!pres->disen) strerr_dief2x(111,name,": is marked disabled") ;
		
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (RELOAD > 1)
		{
			if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc))) strerr_dief1x(111,"-R signal is not allowed to a logger") ;
		}
		if (RELOAD > 1) pres->reload = 1 ;
		
		if (pres->init)
		{
			pres->reload = 0 ;
		//	SIG="-U" ;
		}
		append:		
		if (pres->type == CLASSIC)
		{
			if (!ss_resolve_append(&nclassic,pres)) strerr_diefu2sys(111,"append services selection with: ",name) ;
			cl++ ;
		}
		else
		{
			if (!ss_resolve_append(&nrc,pres)) strerr_diefu2sys(111,"append services selection with: ",name) ;
			rc++;
		}
	}
	
	if (cl)
	{
		VERBO2 strerr_warni1x("sanitize classic services list...") ;
		if(!svc_sanitize(info,&nclassic,envp)) 
			strerr_diefu1x(111,"sanitize classic services list") ;
		VERBO2 strerr_warni1x("start classic services list ...") ;
		if (!svc_send(info,&nclassic,SIG,envp))
			strerr_diefu1x(111,"start classic services list") ;
		VERBO2 strerr_warni3x("switch classic service list of: ",info->treename.s," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch classic service list of: ",info->treename.s," to source") ;
			
		genalloc_deepfree(ss_resolve_t,&nclassic,ss_resolve_free) ;
	} 
	if (rc)
	{
		VERBO2 strerr_warni1x("sanitize atomic services list...") ;
		if (!rc_sanitize(info,&nrc,envp)) 
			strerr_diefu1x(111,"sanitize atomic services list") ;
		if (!empty)
		{
			VERBO2 strerr_warni1x("start atomic services list ...") ;
			if (!rc_start(info,&nrc,SIG,envp)) 
				strerr_diefu1x(111,"start atomic services list ") ;
			VERBO2 strerr_warni3x("switch atomic services list of: ",info->treename.s," to source") ;
			if (!db_switch_to(info,envp,SS_SWSRC))
				strerr_diefu3x(111,"switch atomic services list of: ",info->treename.s," to source") ;
		}
		genalloc_deepfree(ss_resolve_t,&nrc,ss_resolve_free) ;
	}
	stralloc_free(&sares) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	ss_resolve_free(&res) ;
	
	return 0 ;		
}
