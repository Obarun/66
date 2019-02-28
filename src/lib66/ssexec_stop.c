/* 
 * ssexec_stop.c
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
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/selfpipe.h>
#include <skalibs/sig.h>

#include <s6/s6-supervise.h>//s6_svc_ok
#include <s6/config.h>


#include <66/tree.h>
#include <66/db.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/svc.h>
#include <66/rc.h>
#include <66/backup.h>
#include <66/ssexec.h>
#include <66/resolve.h>

#include <stdio.h>


static unsigned int DEADLINE = 0 ;
static unsigned int UNSUP = 0 ;
static char *SIG = "-D" ;
static stralloc sares = STRALLOC_ZERO ;
static genalloc nclassic = GENALLOC_ZERO ;//ss_resolve_t
static genalloc nrc = GENALLOC_ZERO ;//ss_resolve_t

int write_stop_resolve(ss_resolve_t *res,ssexec_t *info)
{
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		strerr_warnwu1sys("set revolve pointer to source") ;
		return 0 ;
	}
	char const *string = res->sa.s ;
	char const *name = string + res->name  ;
	res->reload = 0 ;
	res->run = 1 ;
	res->unsupervise = 0 ;
	res->pid = 0 ;
	VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
	if (!ss_resolve_write(res,sares.s,name))
	{
		VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
		return 0 ;
	}
	if (res->logger)
	{
		VERBO2 strerr_warni2x("Write logger resolve file of: ",name) ;
		if (!ss_resolve_setlognwrite(res,sares.s))
		{
			VERBO1 strerr_warnwu2sys("write logger resolve file of: ",name) ;
			return 0 ;
		}
	}
	
	return 1 ;
}

int svc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	genalloc tounsup = GENALLOC_ZERO ; //stralist
	
	ss_resolve_t_ref pres ;
	
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char *name = pres->sa.s + pres->name ; 
		if (pres->unsupervise) 
		{	
			if (!genalloc_append(ss_resolve_t,&tounsup,pres))
			{
				VERBO1 strerr_warnwu3x("add: ",name," on genalloc") ;
				goto err ;
			}
		}
	}
	
	if (genalloc_len(ss_resolve_t,&tounsup))
	{
		UNSUP = 1 ;
		if (!svc_shutnremove(info,&tounsup,SIG,envp)) return 0 ;
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
		{
			strerr_warnwu1sys("set revolve pointer to source") ;
			return 0 ;
		}
		for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&tounsup) ; i++)
		{
			char const *string = genalloc_s(ss_resolve_t,&tounsup)[i].sa.s ;
			char const *name = string + genalloc_s(ss_resolve_t,&tounsup)[i].name  ;
			// do not remove the resolve file if the daemon was not disabled
			if (genalloc_s(ss_resolve_t,&tounsup)[i].disen)
			{
				genalloc_s(ss_resolve_t,&tounsup)[i].init = 1 ;
				if (!write_stop_resolve(&genalloc_s(ss_resolve_t,&tounsup)[i],info)) goto err ;
				continue ;
			}
			int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
			if (logname > 0) continue ;
			VERBO2 strerr_warni2x("Removing resolve file of: ",name) ;
			if (!ss_resolve_rmfile(&genalloc_s(ss_resolve_t,&tounsup)[i],sares.s,name))
			{
				VERBO1 strerr_warnwu2sys("remove resolve file of: ",name) ;
				goto err ;
			}
		}
	}
	else
	{
		if (!svc_send(info,ga,SIG,envp))
		{
			VERBO1 strerr_warnwu1x("bring down services") ;
			goto err ;
		}
		for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
		{
			genalloc_s(ss_resolve_t,ga)[i].init = 0 ;
			if (!write_stop_resolve(&genalloc_s(ss_resolve_t,ga)[i],info)) goto err ;
		}
	}			
		
	genalloc_free(ss_resolve_t,&tounsup) ;
	return 1 ;
	err: 
		genalloc_free(ss_resolve_t,&tounsup) ;
		return 0 ;
}


int rc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	genalloc tounsup = GENALLOC_ZERO ; //stralist
	
	ss_resolve_t_ref pres ;
	
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char *name = pres->sa.s + pres->name ; 
		if (pres->unsupervise) 
		{	
			if (!genalloc_append(ss_resolve_t,&tounsup,pres))
			{
				VERBO1 strerr_warnwu3x("add: ",name," on genalloc") ;
				goto err ;
			}
		}
	}
	if (genalloc_len(ss_resolve_t,&tounsup))
	{
		UNSUP = 1 ;
		if (!rc_send(info,&tounsup,"-d",envp))
		{
			VERBO1 strerr_warnwu1x("bring down services") ;
			goto err ;
		}
		if (!db_switch_to(info,envp,SS_SWSRC))
		{
			VERBO1 strerr_warnwu3x("switch ",info->treename.s," to backup") ;
			goto err ;
		}
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
		{
			strerr_warnwu1sys("set revolve pointer to source") ;
			return 0 ;
		}
		for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&tounsup) ; i++)
		{
			char const *string = genalloc_s(ss_resolve_t,&tounsup)[i].sa.s ;
			char const *name = string + genalloc_s(ss_resolve_t,&tounsup)[i].name  ;
			// not really necessary because unsup is ignored if the daemon was not disabled
			if (genalloc_s(ss_resolve_t,&tounsup)[i].disen)
			{
				genalloc_s(ss_resolve_t,&tounsup)[i].init = 1 ;
				if (!write_stop_resolve(&genalloc_s(ss_resolve_t,&tounsup)[i],info)) goto err ;
				continue ;
			}
			int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
			if (logname > 0) continue ;
			VERBO2 strerr_warni2x("Removing resolve file of: ",name) ;
			if (!ss_resolve_rmfile(&genalloc_s(ss_resolve_t,&tounsup)[i],sares.s,name))
			{
				VERBO1 strerr_warnwu2sys("remove resolve file of: ",name) ;
				goto err ;
			}
		}
	}
	else
	{	
		if (!rc_send(info,ga,"-d",envp))
		{
			VERBO1 strerr_warnwu1x("bring down services") ;
			goto err ;
		}
		
		for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
		{
			genalloc_s(ss_resolve_t,ga)[i].init = 0 ;
			if (!write_stop_resolve(&genalloc_s(ss_resolve_t,ga)[i],info)) goto err ;
		}
	}
	
	genalloc_free(ss_resolve_t,&tounsup) ;
	return 1 ;
	err: 
		genalloc_free(ss_resolve_t,&tounsup) ;
		return 0 ;
}

int ssexec_stop(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	DEADLINE = 0 ;
	UNSUP = 0 ;
	SIG = "-D" ;
	sares.len = 0 ;
	
	int cl, rc, sigopt, mainunsup ;
			
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
	
	if (info->timeout) DEADLINE = info->timeout ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	
	{
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
		
		for(;*argv;argv++)
		{
			char const *name = *argv ;
			
			ss_resolve_t res = RESOLVE_ZERO ;
			pres = &res ;
			if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2x(110,name," is not enabled") ;
			else if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			
			int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
			
			if (obstr_equal(name,SS_MASTER + 1)) goto run ;
			/** special case, enabling->starting->stopping->disabling
			 * make an orphan service.
			 * check if it's the case and force to stop it*/
			if (!res.run) strerr_dief2x(110,name," : is not running, try 66-start before") ;
			/** always check if the daemon is present or not into the scandir
			 * it can be stopped from different manner (crash,66-scandir signal,..)
			 * without changing the corresponding resolve file */
			if (!s6_svc_ok(res.sa.s + res.runat)) strerr_dief2x(110,name," : is not running") ;
			if (logname > 0)
			{
				if (UNSUP)
				{
					strerr_warnw1x("logger detected - ignoring unsupervise request") ;
				}
				res.unsupervise = 0 ;
			}
			
			if (UNSUP && res.type >= BUNDLE && !res.unsupervise)
			{
				strerr_warnw2x(get_keybyid(res.type)," detected - ignoring unsupervise request") ;
				res.unsupervise = 0 ;
				//UNSUP = 0 ;
			}
				
			if (UNSUP) res.unsupervise = 1 ;
						
			run:
			if (res.type == CLASSIC)
			{
				if (!genalloc_append(ss_resolve_t,&nclassic,&res)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
				if (!ss_resolve_setlognwrite(&res,sares.s)) strerr_diefu2sys(111,"set logger of: ",name) ;
				if (!ss_resolve_addlogger(info,&nclassic)) strerr_diefu2sys(111,"add logger of: ",name) ;
				cl++ ;
			}
			else
			{
				if (!genalloc_append(ss_resolve_t,&nrc,&res)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
				if (!ss_resolve_setlognwrite(&res,sares.s)) strerr_diefu2sys(111,"set logger of: ",name) ;
				if (!ss_resolve_addlogger(info,&nrc)) strerr_diefu2sys(111,"add logger of: ",name) ;
				rc++;
			}
		}
		stralloc_free(&sares) ;
	}
	if (!cl && !rc) ss_resolve_free(pres) ;
	/** rc work */
	if (rc)
	{
		VERBO1 strerr_warni1x("stop atomic services ...") ;
		if (!rc_down(info,&nrc,envp))
			strerr_diefu1x(111,"update atomic services") ;
		VERBO2 strerr_warni3x("switch atomic services of: ",info->treename.s," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename.s," to source") ;
	
		genalloc_deepfree(ss_resolve_t,&nrc,ss_resolve_free) ;
	}
	
	/** svc work */
	if (cl)
	{
		VERBO1 strerr_warni1x("stop classic services ...") ;
		if (!svc_down(info,&nclassic,envp))
			strerr_diefu1x(111,"update classic services") ;
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
