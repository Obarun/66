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

static unsigned int DEADLINE = 0 ;
static unsigned int UNSUP = 0 ;
static char *SIG = "-D" ;

static genalloc nclassic = GENALLOC_ZERO ;//ss_resolve_t
static genalloc nrc = GENALLOC_ZERO ;//ss_resolve_t

int svc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	genalloc tounsup = GENALLOC_ZERO ; //stralist
		
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		ss_resolve_t cp = RESOLVE_ZERO ;
		if (!ss_resolve_copy(&cp,&genalloc_s(ss_resolve_t,ga)[i]))
		{
			VERBO1 strerr_warnwu1sys("copy resolve file") ;
			goto err ;
		}
		if (cp.unsupervise) 
		{	
			if (!ss_resolve_append(&tounsup,&cp)) goto err ;
		}
		ss_resolve_free(&cp) ;
	}
	
	if (genalloc_len(ss_resolve_t,&tounsup))
	{
		UNSUP = 1 ;
		if (!svc_unsupervise(info,&tounsup,SIG,envp)) goto err ;
		genalloc_deepfree(ss_resolve_t,&tounsup,ss_resolve_free) ;
	}
	else
	{
		if (!svc_send(info,ga,SIG,envp))goto err ;
	}
		
	return 1 ;
	err: 
		genalloc_deepfree(ss_resolve_t,&tounsup,ss_resolve_free) ;
		return 0 ;
}

int rc_unsupervise(ssexec_t *info, genalloc *ga,char const *sig,char const *const *envp)
{
	int writein ;
	
	ss_resolve_t_ref pres ;
	stralloc sares = STRALLOC_ZERO ;
	
	if (!access(info->tree.s,W_OK)) writein = SS_DOUBLE ;
	else writein = SS_SIMPLE ;
	
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
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to live") ;
		return 0 ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name  ;
		// do not remove the resolve file if the daemon was not disabled
		if (pres->disen)
		{				
			ss_resolve_setflag(pres,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
			ss_resolve_setflag(pres,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
			VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
			if (!ss_resolve_write(pres,sares.s,name,writein))
			{
				VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
				goto err ;
			}
			VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
			continue ;
		}
		VERBO2 strerr_warni2x("Delete resolve file of: ",name) ;
		if (!ss_resolve_rmfile(pres,sares.s,name,writein))
		{
			VERBO1 strerr_warnwu2sys("delete resolve file of: ",name) ;
			goto err ;
		}
		VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
	}
	
	stralloc_free(&sares) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		return 0 ;
}

int rc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	stralloc sares = STRALLOC_ZERO ;
	genalloc tounsup = GENALLOC_ZERO ; //stralist
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to live") ;
		return 0 ;
	}
	
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		ss_resolve_t cp = RESOLVE_ZERO ;
		if (!ss_resolve_copy(&cp,&genalloc_s(ss_resolve_t,ga)[i]))
		{
			VERBO1 strerr_warnwu1sys("copy resolve file") ;
			goto err ;
		}
		
		if (cp.unsupervise) 
		{	
			if (!ss_resolve_append(&tounsup,&cp)) goto err ;
		}
		ss_resolve_free(&cp) ;
	}
	if (genalloc_len(ss_resolve_t,&tounsup))
	{
		UNSUP = 1 ;
		if (!rc_unsupervise(info,&tounsup,"-d",envp))
		{
			VERBO1 strerr_warnwu1x("stop services") ;
			goto err ;
		}
		genalloc_deepfree(ss_resolve_t,&tounsup,ss_resolve_free) ;
	}
	else
	{	
		if (!rc_send(info,ga,"-d",envp)) goto err ;
	}
	
	stralloc_free(&sares) ;
	return 1 ;
	err: 
		genalloc_deepfree(ss_resolve_t,&tounsup,ss_resolve_free) ;
		stralloc_free(&sares) ;
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
	genalloc gagen = GENALLOC_ZERO ; //ss_resolve_t
	ss_resolve_t_ref pres ;
	
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
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) strerr_diefu1sys(111,"set revolve pointer to source") ;
		
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		ss_resolve_t res = RESOLVE_ZERO ;
		if (!ss_resolve_check(sares.s,name)) strerr_dief2x(110,name," is not enabled") ;
		if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!ss_resolve_add_rdeps(&gagen,&res,sares.s)) strerr_diefu2sys(111,"resolve recursive dependencies of: ",name) ;
		if (!ss_resolve_add_logger(&gagen,sares.s)) strerr_diefu1sys(111,"resolve logger") ;
		ss_resolve_free(&res) ;
	}
		
	stralloc_free(&sares) ;
	genalloc_reverse(ss_resolve_t,&gagen) ;
	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gagen) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,&gagen)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name ;
		size_t earlen = strlen(string + pres->runat) ;
		char earlier[earlen + 8 +1] ;
		memcpy(earlier,string + pres->runat,earlen) ;
		memcpy(earlier + earlen,"/earlier",8) ;
		earlier[earlen + 8] = 0 ;
	
		if (!access(earlier, F_OK))
		{ 
			pres->run = 1 ;
			unlink_void(earlier) ;
		}
		int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		
		if (obstr_equal(name,SS_MASTER + 1))
		{
			if (pres->ndeps) goto append ;
			else continue ;
		}
		/** special case, enabling->starting->stopping->disabling
		 * make an orphan service.
		 * check if it's the case and force to stop it*/
		if (!pres->run && pres->disen) strerr_dief2x(110,name," : is not running, try 66-start before") ;
		if (!pres->unsupervise && !pres->disen) strerr_dief2x(110,name," : is not enabled") ;
		
		/** always check if the daemon is present or not into the scandir
		 * it can be stopped from different manner (crash,66-scandir signal,..)
		 * without changing the corresponding resolve file */
		if (pres->type == LONGRUN || pres->type == CLASSIC)
		{
			if (!s6_svc_ok(string + pres->runat)) strerr_dief2x(110,name," : is not running") ;
		}
		/** logger cannot be unsupervised alone */
		if (logname > 0)
		{
			if (UNSUP && (!ss_resolve_cmp(&gagen,string + pres->logassoc))) strerr_dief1x(111,"logger detected - unsupervise request is not allowed") ;
		}
		if (UNSUP) pres->unsupervise = 1 ;
		if (UNSUP && pres->type >= BUNDLE && !pres->unsupervise)
		{
			VERBO1 strerr_warnw2x(get_keybyid(pres->type)," detected - ignoring unsupervise request") ;
			pres->unsupervise = 0 ;
		}
		append:		
		if (pres->type == CLASSIC)
		{
			if (!ss_resolve_append(&nclassic,pres)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
			cl++ ;
		}
		else
		{
			if (!ss_resolve_append(&nrc,pres)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
			rc++;
		}
	}
	
	genalloc_deepfree(ss_resolve_t,&gagen,ss_resolve_free) ;
	
	/** rc work */
	if (rc)
	{
		VERBO2 strerr_warni1x("stop atomic services ...") ;
		if (!rc_down(info,&nrc,envp))
			strerr_diefu1x(111,"stop atomic services") ;
		VERBO2 strerr_warni3x("switch atomic services of: ",info->treename.s," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename.s," to source") ;
	
		genalloc_deepfree(ss_resolve_t,&nrc,ss_resolve_free) ;
	}
	
	/** svc work */
	if (cl)
	{
		VERBO2 strerr_warni1x("stop classic services ...") ;
		if (!svc_down(info,&nclassic,envp))
			strerr_diefu1x(111,"stop classic services") ;
		VERBO2 strerr_warni3x("switch classic services of: ",info->treename.s," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch classic service of: ",info->treename.s," to source") ;
		
		genalloc_deepfree(ss_resolve_t,&nclassic,ss_resolve_free) ;
	}
		
	if (UNSUP)
	{
		VERBO2 strerr_warnt2x("send signal -an to scandir: ",info->scandir.s) ;
		if (scandir_send_signal(info->scandir.s,"an") <= 0)
			strerr_diefu2sys(111,"send signal to scandir: ", info->scandir.s) ;
	}
	
	return 0 ;		
}
