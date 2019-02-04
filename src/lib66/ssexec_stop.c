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
#include <66/backup.h>
#include <66/ssexec.h>

#include <stdio.h>


static unsigned int DEADLINE = 0 ;
static unsigned int UNSUP = 0 ;
static char *SIG = "-D" ;
static stralloc saresolve = STRALLOC_ZERO ;
genalloc gatoremove = GENALLOC_ZERO ;//stralist
genalloc gaunsup = GENALLOC_ZERO ; //stralist

int svc_release(ssexec_t *info)
{
	if (genalloc_len(stralist,&gaunsup))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gaunsup); i++)
		{
			char const *svname = gaistr(&gaunsup,i) ;
			size_t svnamelen = gaistrlen(&gaunsup,i) ;
	
			char rm[info->scandir.len + 1 + svnamelen + 1] ;
			memcpy(rm,info->scandir.s,info->scandir.len) ;
			rm[info->scandir.len] = '/' ;
			memcpy(rm + info->scandir.len + 1, svname,svnamelen) ;
			rm[info->scandir.len + 1 + svnamelen] = 0 ;
			VERBO3 strerr_warnt2x("unsupervise: ",rm) ;
			if (rm_rf(rm) < 0)
			{
				VERBO3 strerr_warnwu2sys("delete directory: ",rm) ;
				return 0 ;
			}
		}
	}
	
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	
	if (genalloc_len(stralist,&gatoremove))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gatoremove); i++)
		{
			char const *svname = gaistr(&gatoremove,i) ;
			
			if (!resolve_remove_service(saresolve.s,svname))
			{
				VERBO3 strerr_warnwu4sys("delete resolve service file: ",saresolve.s,"/.resolve/",svname) ;
				return 0 ;
			}
		}
	}
	
	return 1 ;
}

int svc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	genalloc tot = GENALLOC_ZERO ; //stralist
		
	for (unsigned int i = 0 ; i < genalloc_len(svstat_t,ga) ; i++) 
	{
		char const *svname = genalloc_s(svstat_t,ga)[i].name ;
		int torm = genalloc_s(svstat_t,ga)[i].remove ;
		int unsup = genalloc_s(svstat_t,ga)[i].unsupervise ;
		
		if (!stra_add(&tot,svname))
		{
			VERBO3 strerr_warnwu3x("add: ",svname," as service to shutdown") ;
			return 0 ;
		}
		
		/** need to remove?*/
		if (torm)
		{
			if (!stra_add(&gatoremove,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to delete") ;
				return 0 ;
			}
		}
		/** unsupervise */
		if (unsup)
		{
			if (!stra_add(&gaunsup,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to unsupervise") ;
				return 0 ;
			}
		}
		/** logger */
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to shutdown") ;
				return 0 ;
			}
			if (torm)
			{
				if (!stra_add(&gatoremove,saresolve.s))
				{
					VERBO3 strerr_warnwu3x("add logger of: ",saresolve.s," as service to delete") ;
					return 0 ;
				}
			}
			if (unsup)
			{
				if (!stra_add(&gaunsup,svname))
				{
					VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to unsupervise") ;
					return 0 ;
				}
			}
		}
	}
	int nargc = 3 + genalloc_len(stralist,&tot) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;

	newargv[m++] = "fake_name" ;
	newargv[m++] = SIG ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
		newargv[m++] = gaistr(&tot,i) ;
	
	newargv[m++] = 0 ;
	
	if (ssexec_svctl(nargc,newargv,envp,info))
	{
		VERBO3 strerr_warnwu1x("shutdown list of services") ;
		return 0 ;
	}
		
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("delete remove resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"remove"))
		{
			VERBO3 strerr_warnwu3sys("delete resolve file",saresolve.s,"/remove") ;
			return 0 ;
		}
	}
	
	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
	
}
int rc_release(ssexec_t *info)
{

	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	if (genalloc_len(stralist,&gatoremove))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gatoremove); i++)
		{
			char const *svname = gaistr(&gatoremove,i) ;
			
			if (!resolve_remove_service(saresolve.s,svname))
			{
				VERBO3 strerr_warnwu4sys("delete resolve service file: ",saresolve.s,"/.resolve/",svname) ;
				return 0 ;
			}
		}
	}
	
	return 1 ;
}

int rc_down(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	size_t treenamelen = strlen(info->treename) ;
	
	genalloc tot = GENALLOC_ZERO ; //stralist

	for (unsigned int i = 0 ; i < genalloc_len(svstat_t,ga) ; i++) 
	{
		
		char const *svname = genalloc_s(svstat_t,ga)[i].name ;
		int torm = genalloc_s(svstat_t,ga)[i].remove ;
		
		if (!stra_add(&tot,svname))
		{
			VERBO3 strerr_warnwu3x("add: ",svname," as service to shutdown") ;
			return 0 ;
		}
		
		/** need to remove?*/
		if (torm)
		{
			if (!stra_add(&gatoremove,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to delete") ;
				return 0 ;
			}
		}
		/** logger */
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to shutdown") ;
				return 0 ;
			}
			if (torm)
			{
				if (!stra_add(&gatoremove,saresolve.s))
				{
					VERBO3 strerr_warnwu3x("add logger of: ",saresolve.s," as service to delete") ;
					return 0 ;
				}
			}
		}
	}
	
	char db[info->livetree.len + 1 + treenamelen + 1] ;
	memcpy(db,info->livetree.s,info->livetree.len) ;
	db[info->livetree.len] = '/' ;
	memcpy(db + info->livetree.len + 1, info->treename, treenamelen) ;
	db[info->livetree.len + 1 + treenamelen] = 0 ;
	
	if (!db_ok(info->livetree.s, info->treename))
		strerr_dief2x(111,db," is not initialized") ;
		
	int nargc = 3 + genalloc_len(stralist,&tot) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	newargv[m++] = "-d" ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&tot) ; i++)
			newargv[m++] = gaistr(&tot,i) ;
		
	newargv[m++] = 0 ;
	
	if (ssexec_dbctl(nargc,newargv,envp,info))
	{
		VERBO3 strerr_warnwu1x("bring down service list") ;
		return 0 ;
	}
	
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("delete remove resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"remove"))
		{
			VERBO3 strerr_warnwu3sys("delete resolve file",saresolve.s,"/remove") ;
			return 0 ;
		}
	}

	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
}

int ssexec_stop(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	// be sure that the global var are set correctly
	DEADLINE = 0 ;
	UNSUP = 0 ;
	SIG = "-D" ;
	saresolve = stralloc_zero ;
	gatoremove = genalloc_zero ;//stralist
	gaunsup = genalloc_zero ; //stralist

	int r, rb, sigopt ;
		
	stralloc svok = STRALLOC_ZERO ;
	
	genalloc nclassic = GENALLOC_ZERO ;
	genalloc nrc = GENALLOC_ZERO ;
	
	sigopt = 0 ;
	
	//PROG = "66-stop" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">uXK", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;

			switch (opt)
			{
				case 'u' :	UNSUP = 1 ; break ;
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
	
	if (!stralloc_catb(&svok,info->scandir.s,info->scandir.len)) retstralloc(111,"main") ; 
	if (!stralloc_cats(&svok,"/")) retstralloc(111,"main") ; 
	size_t svoklen = svok.len ;
	
	{
		stralloc type = STRALLOC_ZERO ;
		svstat_t svsrc = SVSTAT_ZERO ;
		
		for(;*argv;argv++)
		{
			svsrc.type = 0 ;
			svsrc.name = 0 ;
			svsrc.namelen = 0 ;
			svsrc.remove = 0 ;
			svsrc.unsupervise = 0 ;
			svok.len = svoklen ;
			
			if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
				strerr_diefu1x(111,"set revolve pointer to source") ;
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb < 0)
			{
				if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_BACK))
					strerr_diefu1x(111,"set revolve pointer to backup") ;
				r = resolve_read(&type,saresolve.s,*argv,"type") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ; 
				if (r > 0) svsrc.remove = 1 ;
				if (r <= 0) strerr_dief2x(111,*argv,": is not enabled") ;
			}
			if (rb > 0)
			{
				svsrc.type = get_enumbyid(type.s,key_enum_el) ;
				svsrc.name = *argv ;
				svsrc.namelen = strlen(*argv) ;
				r = resolve_read(&type,saresolve.s,*argv,"remove") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ;
				if (r > 0) svsrc.remove = 1 ;
			/*	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_BACK))
					strerr_diefu1x(111,"set revolve pointer to backup") ;
				r = resolve_read(&type,saresolve.s,*argv,"type") ;
				if (r <= 0 && !UNSUP) strerr_dief3x(111,"service: ",*argv," is not running, you can only start it") ;*/
			}
			if (svsrc.type == CLASSIC)
			{
				if (!stralloc_cats(&svok,*argv)) retstralloc(111,"main") ; 
				if (!stralloc_0(&svok)) retstralloc(111,"main") ; 
				r = s6_svc_ok(svok.s) ;
				if (r < 0) strerr_diefu2sys(111,"check ", svok.s) ;
				//if (!r)	svsrc.remove = 1 ;
				if (!r) strerr_dief3x(111,"service: ",*argv," is not running, you can only start it") ;
			}
			
			if (UNSUP) svsrc.unsupervise = 1 ;
			if (svsrc.remove) svsrc.unsupervise = 1 ;
			
			if (svsrc.type == CLASSIC)
			{
				if (!genalloc_append(svstat_t,&nclassic,&svsrc)) strerr_diefu3x(111,"add: ",*argv," on genalloc") ;				
			}
			else
			{
				if (!genalloc_append(svstat_t,&nrc,&svsrc)) strerr_diefu3x(111,"add: ",*argv," on genalloc") ;
			}
		}
		stralloc_free(&type) ;
		stralloc_free(&svok) ;
	}
	
	/** rc work */
	if (genalloc_len(svstat_t,&nrc))
	{
		VERBO2 strerr_warni1x("stop rc services ...") ;
		if (!rc_down(info,&nrc,envp))
			strerr_diefu1x(111,"update rc services") ;
		VERBO2 strerr_warni1x("release rc services ...") ;
		if (!rc_release(info))
			strerr_diefu1x(111,"release rc services") ;
		VERBO2 strerr_warni3x("switch rc services of: ",info->treename," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename," to source") ;
	}
	
	/** svc work */
	if (genalloc_len(svstat_t,&nclassic))
	{
		VERBO2 strerr_warni1x("stop svc services ...") ;
		if (!svc_down(info,&nclassic,envp))
			strerr_diefu1x(111,"update svc services") ;
		VERBO2 strerr_warni1x("release svc services ...") ;
		if (!svc_release(info))
			strerr_diefu1x(111,"release svc services ...") ;
		VERBO2 strerr_warni3x("switch svc services of: ",info->treename," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch svc service of: ",info->treename," to source") ;
		
	}
		
	if (UNSUP || genalloc_len(stralist,&gatoremove))
	{
		VERBO2 strerr_warnt2x("send signal -an to scandir: ",info->scandir.s) ;
		if (scandir_send_signal(info->scandir.s,"an") <= 0)
			strerr_diefu2sys(111,"send signal to scandir: ", info->scandir.s) ;
	}
	
	genalloc_free(svstat_t,&nrc) ;
	genalloc_free(svstat_t,&nclassic) ;
	
	return 0 ;		
}
