/* 
 * ssexec_stop.c
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

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>//s6_svc_ok

#include <66/db.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/rc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/state.h>

static unsigned int DEADLINE = 0 ;
static unsigned int UNSUP = 0 ;
static char *SIG = "-D" ;

static ss_resolve_graph_t graph_unsup_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_cl = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_unsup_rc = RESOLVE_GRAPH_ZERO ;
static ss_resolve_graph_t graph_rc = RESOLVE_GRAPH_ZERO ;

int svc_down(ssexec_t *info, char const *const *envp)
{
	unsigned int reverse = 1 ;
	int r ;

	if (genalloc_len(ss_resolve_t,&graph_unsup_cl.name))
	{
		UNSUP = 1 ;
		r = ss_resolve_graph_publish(&graph_unsup_cl,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_unsupervise(info,&graph_unsup_cl.sorted,SIG,envp)) goto err ;
	}
	else
	{
		r = ss_resolve_graph_publish(&graph_cl,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!svc_send(info,&graph_cl.sorted,SIG,envp)) goto err ;
	}
	ss_resolve_graph_free(&graph_unsup_cl) ;	
	ss_resolve_graph_free(&graph_cl) ;	
	return 1 ;
	err: 
		ss_resolve_graph_free(&graph_unsup_cl) ;	
		ss_resolve_graph_free(&graph_cl) ;	
		return 0 ;
}

int rc_unsupervise(ssexec_t *info, genalloc *ga,char const *sig,char const *const *envp)
{
	ss_resolve_t_ref pres ;
	ss_state_t sta = STATE_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	stralloc sasta = STRALLOC_ZERO ;
	
	if (!rc_send(info,ga,sig,envp))
	{
		VERBO1 strerr_warnwu1x("stop services") ;
		goto err ;
	}
	if (!db_switch_to(info,envp,SS_SWSRC))
	{
		VERBO1 strerr_warnwu3x("switch ",info->treename.s," to source") ;
		goto err ;
	}
	if (!ss_resolve_pointo(&sasta,info,SS_NOTYPE,SS_RESOLVE_STATE))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to state") ;
		return 0 ;
	}
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name  ;
		// do not remove the resolve file if the daemon was not disabled
		if (!pres->disen)
		{				
			VERBO2 strerr_warni2x("Delete resolve file of: ",name) ;
			ss_resolve_rmfile(pres,sares.s,name) ;
			VERBO2 strerr_warni2x("Delete state file of: ",name) ;
			ss_state_rmfile(sasta.s,name) ;
		}
		else
		{
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
			ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
			VERBO2 strerr_warni2x("Write state file of: ",name) ;
			if (!ss_state_write(&sta,sasta.s,name))
			{
				VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
				goto err ;
			}
		}
		VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
	}
	
	stralloc_free(&sares) ;
	stralloc_free(&sasta) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		stralloc_free(&sasta) ;
		return 0 ;
}

int rc_down(ssexec_t *info, char const *const *envp)
{
	unsigned int reverse = 1 ;
	int r ;
		
	if (genalloc_len(ss_resolve_t,&graph_unsup_rc.name))
	{
		UNSUP = 1 ;
		r = ss_resolve_graph_publish(&graph_unsup_rc,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!rc_unsupervise(info,&graph_unsup_rc.sorted,"-d",envp)) goto err ;
	}
	else
	{
		r = ss_resolve_graph_publish(&graph_rc,reverse) ;
		if (r < 0 || !r)
		{
			VERBO1 strerr_warnwu1sys("publish service graph") ;
			goto err ;
		}
		if (!rc_send(info,&graph_rc.sorted,"-d",envp)) goto err ;
	}
	
	ss_resolve_graph_free(&graph_unsup_rc) ;
	ss_resolve_graph_free(&graph_rc) ;
	return 1 ;
	err: 
		ss_resolve_graph_free(&graph_unsup_rc) ;
		ss_resolve_graph_free(&graph_rc) ;
		return 0 ;
}

int ssexec_stop(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	DEADLINE = 0 ;
	UNSUP = 0 ;
	SIG = "-D" ;
		
	if (info->timeout) DEADLINE = info->timeout ;
	
	int cl, rc, sigopt, mainunsup ;
	stralloc sares = STRALLOC_ZERO ;
	stralloc sasta = STRALLOC_ZERO ;
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t
	ss_resolve_t_ref pres ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	cl = rc = sigopt = mainunsup = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">uXK", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;

			switch (opt)
			{
				case 'u' :	UNSUP = 1 ; mainunsup = 1 ; break ;
				case 'X' :	if (sigopt) exitusage(usage_stop) ; sigopt = 1 ; SIG = "-X" ; break ;
				case 'K' :	if (sigopt) exitusage(usage_stop) ; sigopt = 1 ; SIG = "-K" ; break ;
				default : exitusage(usage_stop) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(usage_stop) ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
		
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(sares.s,name)) strerr_dief2x(110,name," is not enabled") ;
		if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!ss_resolve_append(&gares,&res)) strerr_diefu2sys(111,"append resolve file of: ",name) ;
	}
	
	if (!ss_resolve_pointo(&sasta,info,SS_NOTYPE,SS_RESOLVE_STATE)) strerr_diefu1sys(111,"set revolve pointer to state") ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		int unsup = 0 , reverse = 1 ;
		pres = &genalloc_s(ss_resolve_t,&gares)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name ;
	
		if (!ss_state_check(sasta.s,name)) strerr_dief2x(110,name," : is not initialized") ;
		else if (!ss_state_read(&sta,sasta.s,name)) strerr_diefu2sys(111,"read state file of: ",name) ;
	
		int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		
		if (obstr_equal(name,SS_MASTER + 1))
		{
			if (pres->ndeps) goto append ;
			else continue ;
		}
		
		/** logger cannot be unsupervised alone */
		if (logname > 0 && (!ss_resolve_cmp(&gares,string + pres->logassoc)))
		{
			if (UNSUP) strerr_dief1x(111,"logger detected - unsupervise request is not allowed") ;
		}
		if (UNSUP) unsup = 1 ;
		if (sta.unsupervise) unsup = 1 ;
		append:		
		if (pres->type == CLASSIC)
		{
			if (unsup)
			{
				if (!ss_resolve_graph_build(&graph_unsup_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
				if (!ss_resolve_add_logger(&graph_unsup_cl.name,sares.s)) 
					strerr_diefu1sys(111,"append service selection with logger") ;
			}
			if (!ss_resolve_graph_build(&graph_cl,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
				strerr_diefu1sys(111,"build services graph") ;
			cl++ ;
		}
		else
		{
			if (unsup)
			{
				if (!ss_resolve_graph_build(&graph_unsup_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) 
					strerr_diefu1sys(111,"build services graph") ;
				if (!ss_resolve_add_logger(&graph_unsup_rc.name,sares.s)) 
					strerr_diefu1sys(111,"append service selection with logger") ;
			}
			if (!ss_resolve_graph_build(&graph_rc,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse))
				strerr_diefu1sys(111,"build services graph") ;
			rc++;
		}
	}
	
	/** rc work */
	if (rc)
	{
		VERBO2 strerr_warni1x("stop atomic services ...") ;
		if (!rc_down(info,envp))
			strerr_diefu1x(111,"stop atomic services") ;
		VERBO2 strerr_warni3x("switch atomic services of: ",info->treename.s," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename.s," to source") ;

	}
	
	/** svc work */
	if (cl)
	{
		VERBO2 strerr_warni1x("stop classic services ...") ;
		if (!svc_down(info,envp))
			strerr_diefu1x(111,"stop classic services") ;
		VERBO2 strerr_warni3x("switch classic services of: ",info->treename.s," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch classic service of: ",info->treename.s," to source") ;
	}
		
	if (UNSUP)
	{
		VERBO2 strerr_warnt2x("send signal -an to scandir: ",info->scandir.s) ;
		if (scandir_send_signal(info->scandir.s,"an") <= 0)
			strerr_diefu2sys(111,"send signal to scandir: ", info->scandir.s) ;
	}
	stralloc_free(&sares) ;
	stralloc_free(&sasta) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	
	return 0 ;		
}
