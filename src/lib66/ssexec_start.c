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
#include <66/state.h>

#include <s6/s6-supervise.h>

static int empty = 0 ;
static unsigned int RELOAD = 0 ;
static unsigned int DEADLINE = 0 ;
static char *SIG = "-U" ;

static genalloc nclassic = GENALLOC_ZERO ; //resolve_t type
static genalloc nrc = GENALLOC_ZERO ; //resolve_t type
static ss_resolve_graph_t graph_init_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_reload_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_init_rc = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_reload_rc = RESOLVE_GRAPH_ZERO ;

int svc_sanitize(ssexec_t *info, char const *const *envp)
{
	unsigned int reverse = 0 ;
	int r ;
	stralloc sares = STRALLOC_ZERO ;
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_SRC))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
		goto err;
	}	
	if (genalloc_len(ss_resolve_t,&graph_reload_cl.name))
	{	
		//reverse = 1 ;
		r = ss_resolve_graph_publish(&graph_reload_cl,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_unsupervise(info,&graph_reload_cl.sorted,"-D",envp))	goto err ;
		genalloc_reverse(ss_resolve_t,&graph_reload_cl.sorted) ;
		if (!svc_init(info,sares.s,&graph_reload_cl.sorted))
		{
			VERBO1 strerr_warnwu1x("iniatiate service list") ;
			goto err ;
		}
		goto end ;
	}
	if (genalloc_len(ss_resolve_t,&graph_init_cl.name))
	{
		r = ss_resolve_graph_publish(&graph_init_cl,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_init(info,sares.s,&graph_init_cl.sorted))
		{
			VERBO1 strerr_warnwu1x("iniatiate service list") ;
			goto err ;
		}
	}
	
	end:
	stralloc_free(&sares) ;
	ss_resolve_graph_free(&graph_reload_cl) ;
	ss_resolve_graph_free(&graph_init_cl) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		ss_resolve_graph_free(&graph_reload_cl) ;
		ss_resolve_graph_free(&graph_init_cl) ;
		return 0 ;
}

int rc_sanitize(ssexec_t *info, char const *const *envp)
{
	int r, reverse = 1 ;
	stralloc sares = STRALLOC_ZERO ;
	
	char db[info->livetree.len + 1 + info->treename.len + 1] ;
	memcpy(db,info->livetree.s,info->livetree.len) ;
	db[info->livetree.len] = '/' ;
	memcpy(db + info->livetree.len + 1, info->treename.s, info->treename.len) ;
	db[info->livetree.len + 1 + info->treename.len] = 0 ;
	
	if (!db_ok(info->livetree.s,info->treename.s) || genalloc_len(ss_resolve_t,&graph_init_rc.name))
	{
		r = rc_init(info,envp) ;
		if (!r) goto err ;
		else if (r > 1) { empty = 1 ; goto end ; }
	}
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_SRC))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
		goto err;
	}
	
	if (genalloc_len(ss_resolve_t,&graph_reload_rc.name))
	{	
		r = ss_resolve_graph_publish(&graph_reload_rc,reverse) ;
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
	
		if (!rc_send(info,&graph_reload_rc.sorted,"-d",envp)) goto err ;
	}
	end:
	ss_resolve_graph_free(&graph_reload_rc) ;
	ss_resolve_graph_free(&graph_init_rc) ;
	stralloc_free(&sares) ;
	return 1 ;

	err:
		ss_resolve_graph_free(&graph_reload_rc) ;
		ss_resolve_graph_free(&graph_init_rc) ;
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
	stralloc sasta = STRALLOC_ZERO ;
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t	
	ss_resolve_t_ref pres ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
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
	
	
	if (!ss_resolve_pointo(&sasta,info,SS_NOTYPE,SS_RESOLVE_STATE)) strerr_diefu1sys(111,"set revolve pointer to state") ;
	/** the tree may not initialized already, check it and create
	 * the live directory if it's the case */
	if (!scan_mode(sasta.s,S_IFDIR))
		if (!ss_resolve_create_live(info)) strerr_diefu1sys(111,"create live state") ;
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(sares.s,name)) strerr_dief2x(110,name," is not enabled") ;
		if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!ss_resolve_append(&gares,&res)) strerr_diefu2sys(111,"append services selection with: ",name) ;
	}
		
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		int init = 0 ;
		int reload = 0 ;
		int reverse = 0 ;
		pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
		char *string = pres->sa.s ;
		char *name = string + pres->name ;
		logname = 0 ;
		if (!ss_state_check(sasta.s,name)) 
		{
			init = 1 ;
			goto append ;
		}
		else if (!ss_state_read(&sta,sasta.s,name)) strerr_diefu2sys(111,"read state file of: ",name) ;
		
		if (obstr_equal(name,SS_MASTER + 1)) goto append ;
		
		if (!pres->disen){ VERBO1 strerr_dief3x(111,"service: ",name," was disabled, you can only stop it") ; }
		
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc)))
		{
			if (RELOAD > 1) strerr_dief1x(111,"-R signal is not allowed to a logger") ;
			if (sta.init) reverse = 1 ;
		}
		if (RELOAD > 1) reload = 1 ;
	
		if (sta.init){ reload = 0 ; init = 1 ; }
		
		append:		
		if (pres->type == CLASSIC)
		{
			if (reload)
			{
				reverse = 1 ;
				if (!ss_resolve_graph_build(&graph_reload_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
			}
			else if (init)
			{
				if (!ss_resolve_graph_build(&graph_init_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
			}
			if (!ss_resolve_append(&nclassic,pres)) strerr_diefu2sys(111,"append services selection with: ",name) ;
			cl++ ;
		}
		else
		{
			if (reload)
			{
				reverse = 1 ;
				if (!ss_resolve_graph_build(&graph_reload_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
			}
			else if (init)
			{
				if (!ss_resolve_graph_build(&graph_init_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
			}
			if (!ss_resolve_append(&nrc,pres)) strerr_diefu2sys(111,"append services selection with: ",name) ;
			rc++;
		}
	}
	
	if (cl)
	{
		VERBO2 strerr_warni1x("sanitize classic services list...") ;
		if(!svc_sanitize(info,envp)) 
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
		if (!rc_sanitize(info,envp)) 
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
	stralloc_free(&sasta) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	ss_resolve_free(&res) ;
	
	return 0 ;		
}
