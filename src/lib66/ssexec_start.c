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

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/tai.h>

#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>

#include <s6/s6-supervise.h>//s6_svstatus_t
#include <s6/ftrigw.h>
#include <s6/config.h>

#include <66/tree.h>
#include <66/db.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/backup.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/rc.h>

#include <stdio.h>

static unsigned int RELOAD = 0 ;
static unsigned int DEADLINE = 0 ;
static char *SIG = "-U" ;
static stralloc sares = STRALLOC_ZERO ;
static genalloc nclassic = GENALLOC_ZERO ; //resolve_t type
static genalloc nrc = GENALLOC_ZERO ; //resolve_t type

int svc_start(ssexec_t *info,genalloc *ga,char const *const *envp)
{	
	if (!svc_send(info,ga,SIG,envp))
	{
		VERBO1 strerr_warnwu1x("start services") ;
		return 0 ;
	}

	return 1 ;
}

int svc_sanitize(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	
	genalloc toreload = GENALLOC_ZERO ; //ss_resolve_t
	genalloc toinit = GENALLOC_ZERO ; //ss_resolve_t
	
	ss_resolve_t_ref pres ;
	
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char *name = pres->sa.s + pres->name ; 
		if (pres->reload) 
		{	
			if (!genalloc_append(ss_resolve_t,&toreload,pres))
			{
				VERBO1 strerr_warnwu3x("add: ",name," on genalloc") ;
				goto err ;
			}
		}
		else if (pres->init)
		{
			if (!genalloc_append(ss_resolve_t,&toinit,pres))
			{
				VERBO1 strerr_warnwu3x("add: ",name," on genalloc") ;
				goto err ;
			}
		}
	}
		
	if (genalloc_len(ss_resolve_t,&toreload))
	{
		if (!svc_unsupervise(info,&toreload,"-D",envp))	goto err ;
		genalloc_cat(ss_resolve_t,&toinit,&toreload) ;
	}
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_SRC))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
		goto err;
	}
	if (genalloc_len(ss_resolve_t,&toinit))
	{
		if (!svc_init(info,sares.s,&toinit))
		{
			VERBO1 strerr_warnwu1x("iniatiate svc service list") ;
			goto err ;
		}
	}
	
	genalloc_free(ss_resolve_t,&toreload) ;
	genalloc_free(ss_resolve_t,&toinit) ;
	
	return 1 ;
	
	err:
		genalloc_free(ss_resolve_t,&toreload) ;
		genalloc_free(ss_resolve_t,&toinit) ;
		return 0 ;
}

int rc_sanitize(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	genalloc toreload = GENALLOC_ZERO ; //stralist
	
	ss_resolve_t_ref pres ;
		
	char db[info->livetree.len + 1 + info->treename.len + 1] ;
	memcpy(db,info->livetree.s,info->livetree.len) ;
	db[info->livetree.len] = '/' ;
	memcpy(db + info->livetree.len + 1, info->treename.s, info->treename.len) ;
	db[info->livetree.len + 1 + info->treename.len] = 0 ;
	
	if (!db_ok(info->livetree.s,info->treename.s))
		if (!rc_init(info,ga,envp)) return 0 ;
	

	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char *name = pres->sa.s + pres->name ; 
		if (pres->reload)
		{
			if (!genalloc_append(ss_resolve_t,&toreload,pres)){
				VERBO1 strerr_warnwu3x("add: ",name," on genalloc") ;
				goto err ;
			}
		}
	}
	if (genalloc_len(ss_resolve_t,&toreload))
	{		
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
		
		if (!rc_send(info,&toreload,"-d",envp))
		{
			VERBO1 strerr_warnwu1x("stop services") ;
			goto err ;
		}
	}

	genalloc_free(ss_resolve_t,&toreload) ;
	stralloc_free(&sares) ;
	return 1 ;

	err:
		genalloc_free(ss_resolve_t,&toreload) ;
		stralloc_free(&sares) ;
		return 0 ;
}

int rc_start(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	char *s = 0 ;
	int r = db_find_compiled_state(info->livetree.s,info->treename.s) ;
	if (r)
	{
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu3x(111,"switch: ",info->treename.s," to source") ;
	}
	
	if (RELOAD == 1) s = "-r" ;
	else s = "-u" ;
	if (!rc_send(info,ga,s,envp))
	{
		VERBO1 strerr_warnwu1x("start services") ;
		return 0 ;
	}

	return 1 ;
}

int ssexec_start(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	RELOAD = 0 ;
	SIG = "-U" ;
	sares.len = 0 ;
	
	if (info->timeout) DEADLINE = info->timeout ;
	
	int cl, rc, logname ;
	genalloc gagen = GENALLOC_ZERO ; //ss_resolve_t	
	ss_resolve_t_ref pres ;
	
	cl = rc = logname = 0 ;
	
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

	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	for (;*argv;argv++)
	{
		char const *name = *argv ;
		ss_resolve_t res = RESOLVE_ZERO ;
		if (!ss_resolve_check(info,name,SS_RESOLVE_LIVE)) strerr_dief2x(110,name," is not enabled") ;
		else if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!genalloc_append(ss_resolve_t,&gagen,&res)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
		if (!ss_resolve_add_deps(&gagen,&res,info)) strerr_diefu2sys(111,"resolve recursive dependencies of: ",name) ;
	}
		
	stralloc_free(&sares) ;
	
	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gagen) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,&gagen)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name ;
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
		if (obstr_equal(name,SS_MASTER + 1)) goto run ;
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
		else if (!pres->disen) strerr_dief2x(111,name,": is not enabled") ;
		
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (RELOAD > 1)
		{
			if (logname > 0 && (!ss_resolve_cmp(&gagen,string + pres->logassoc))) strerr_dief1x(111,"-R signal is not allowed to a logger") ;
		}
		if (RELOAD > 1) pres->reload = 1 ;
					
		if (pres->init) pres->reload = 0 ;
		
		run:		
		if (pres->type == CLASSIC)
		{
			if (!genalloc_append(ss_resolve_t,&nclassic,pres)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
			cl++ ;
		}
		else
		{
			if (!genalloc_append(ss_resolve_t,&nrc,pres)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
			rc++;
		}
	}
			
	if (cl)
	{
		VERBO2 strerr_warni1x("sanitize classic services list...") ;
		if(!svc_sanitize(info,&nclassic,envp)) 
			strerr_diefu1x(111,"sanitize classic services list") ;
		VERBO2 strerr_warni1x("start classic services list ...") ;
		if(!svc_start(info,&nclassic,envp)) 
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
		VERBO2 strerr_warni1x("start atomic services list ...") ;
		if (!rc_start(info,&nrc,envp)) 
			strerr_diefu1x(111,"start atomic services list ") ;
		VERBO2 strerr_warni3x("switch atomic services list of: ",info->treename.s," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu3x(111,"switch atomic services list of: ",info->treename.s," to source") ;
		
		genalloc_deepfree(ss_resolve_t,&nrc,ss_resolve_free) ;
	}
	
	return 0 ;		
}
