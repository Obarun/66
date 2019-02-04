/* 
 * ssexec_start.c
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

//#include <stdio.h>

static unsigned int RELOAD = 0 ;
static unsigned int DEADLINE = 0 ;
static stralloc saresolve = STRALLOC_ZERO ;
static char *SIG = "-U" ;
static genalloc DOWNFILE = GENALLOC_ZERO ; //stralist

int svc_shutnremove(ssexec_t *info, genalloc *ga, char const *const *envp)
{
	int r ;

	stralloc log = STRALLOC_ZERO ;
	genalloc tot = GENALLOC_ZERO ; //stralist
	
	for (unsigned int i = 0 ; i < genalloc_len(svstat_t,ga) ; i++) 
	{
		
		char const *svname = genalloc_s(svstat_t,ga)[i].name ;
		
		if (!stra_add(&tot,svname))
		{
			VERBO3 strerr_warnwu3x("add: ",svname," as service to shutdown") ;
			return 0 ;
		}
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		r = resolve_read(&saresolve,saresolve.s,svname,"logger") ;
		
		if (r) 
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add logger of: ",svname," as service to shutdown") ;
				return 0 ;
			}
			
		}
	}	
	
	int nargc = 3 + genalloc_len(stralist,&tot) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;

	newargv[m++] = "fake_name" ;
	newargv[m++] = "-D" ;
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
		newargv[m++] = gaistr(&tot,i) ;
			
	newargv[m++] = 0 ;
	
	if (ssexec_svctl(nargc,newargv,envp,info))
	{
		VERBO3 strerr_warnwu1x("shutdown list of services") ;
		return 0 ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
	{
		size_t namelen = gaistrlen(&tot,i) ;
		char const *name = gaistr(&tot,i)  ;
		char svscan[info->scandir.len + 1 + namelen + 1] ;
		memcpy(svscan,info->scandir.s,info->scandir.len) ;
		svscan[info->scandir.len] = '/' ;
		memcpy(svscan + info->scandir.len + 1,name, namelen) ;
		svscan[info->scandir.len + 1 + namelen] = 0 ;
		
		VERBO3 strerr_warnt2x("delete: ", svscan) ;
		if (rm_rf(svscan) < 0)
		{
			VERBO3 strerr_warnwu2sys("delete: ",svscan) ;
			return 0 ;
		}
	}
	stralloc_free(&log) ;
	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
}
		
int svc_start(ssexec_t *info,genalloc *ga,char const *const *envp)
{	
	genalloc tot = genalloc_zero ; //stralist
	
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{	
		if (!stra_add(&tot,genalloc_s(svstat_t,ga)[i].name))
		{
			VERBO3 strerr_warnwu3x("add: ",genalloc_s(svstat_t,ga)[i].name," as service to start") ;
			return 0 ;
		}
		
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,genalloc_s(svstat_t,ga)[i].name,"logger"))
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to shutdown") ;
				return 0 ;
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
		VERBO3 strerr_warnwu1x("bring up services") ;
		return 0 ;
	}
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}

	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		if (genalloc_s(svstat_t,ga)[i].reload)
		{
			VERBO3 strerr_warnt2x("delete reload resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
			if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"reload"))
			{
				VERBO3 strerr_warnwu3sys("delete resolve file",saresolve.s,"/reload") ;
				return 0 ;
			}
		}
	}

	return 1 ;
}

int svc_sanitize(ssexec_t *info, genalloc *ga, char const *const *envp)
{
	genalloc toreload = GENALLOC_ZERO ; //svstat_t
	genalloc toinit = GENALLOC_ZERO ; //svstat_t
	
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		if (genalloc_s(svstat_t,ga)[i].reload)
		{
			if (!genalloc_cat(svstat_t,&toreload,ga))
			{
				VERBO3 strerr_warnwu1x("append genalloc") ;
				return 0 ;
			}
		}
		if (genalloc_s(svstat_t,ga)[i].init)
		{
			if (!genalloc_cat(svstat_t,&toinit,ga))
			{
				VERBO3 strerr_warnwu1x("append genalloc") ;
				return 0 ;
			}
		}
	}
	if (genalloc_len(svstat_t,&toinit))
	{
		if (!resolve_pointo(&saresolve,info,CLASSIC,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		VERBO3 strerr_warnt1x("initiate svc service list ...") ;
		if (!svc_init(info->scandir.s, saresolve.s, &toinit))
		{
			VERBO3 strerr_warnwu1x("iniatiate svc service list") ;
			return 0 ;
		}
	}
	if (genalloc_len(svstat_t,&toreload))
	{
		VERBO3 strerr_warnt1x("shutdown service before reload it ...") ;
		if (!svc_shutnremove(info,&toreload,envp))
		{
			VERBO3 strerr_warnwu1x("down list of service") ;
			return 0 ;
		}
		if (!resolve_pointo(&saresolve,info,CLASSIC,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		VERBO3 strerr_warnt1x("initiate svc services list ...") ;
		if (!svc_init(info->scandir.s, saresolve.s, &toreload))
		{
			VERBO3 strerr_warnwu1x("iniatiate svc service list") ;
			return 0 ;
		}
	}
	
	return 1 ;
}

int rc_init(ssexec_t *info, char const *const *envp)
{
	int r ;
	
	int nargc = 4 ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	newargv[m++] = "-d" ;
	newargv[m++] = info->treename ;
	newargv[m++] = 0 ;
				
	if (ssexec_init(nargc,newargv,envp,info))
	{
		strerr_warnwu2x("init rc services for tree: ",info->treename) ;
		return 0 ;
	}
	
	VERBO3 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
	r = s6_svc_writectl(info->scandir.s, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",info->scandir.s, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		return 0 ;
	}
	
	return 1 ;
}

int rc_sanitize(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	genalloc toreload = GENALLOC_ZERO ; //stralist
	
	size_t treenamelen = strlen(info->treename) ;
	
	char db[info->livetree.len + 1 + treenamelen + 1] ;
	memcpy(db,info->livetree.s,info->livetree.len) ;
	db[info->livetree.len] = '/' ;
	memcpy(db + info->livetree.len + 1, info->treename, treenamelen) ;
	db[info->livetree.len + 1 + treenamelen] = 0 ;
	
	if (!db_ok(info->livetree.s,info->treename))
		if (!rc_init(info,envp)) return 0 ;
	
		
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		if (genalloc_s(svstat_t,ga)[i].reload)
		{
			if (!stra_add(&toreload,genalloc_s(svstat_t,ga)[i].name))
			{
				VERBO3 strerr_warnwu3x("add: ",genalloc_s(svstat_t,ga)[i].name," as service to reload") ;
				return 0 ;
			}
		}
	}
	if (genalloc_len(stralist,&toreload))
	{
		
		if (!db_switch_to(info,envp,SS_SWBACK))
		{
			VERBO3 strerr_warnwu3x("switch ",info->treename," to backup") ;
			return 0 ;
		}
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (!db_compile(saresolve.s,info->tree.s, info->treename,envp))
		{
			VERBO3 strerr_diefu4x(111,"compile ",saresolve.s,"/",info->treename) ;
			return 0 ;
		}
		if (!db_switch_to(info,envp,SS_SWSRC))
		{
			VERBO3 strerr_warnwu3x("switch ",info->treename," to backup") ;
			return 0 ;
		}
		
		int nargc = 3 + genalloc_len(stralist,&toreload) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		newargv[m++] = "-d" ;

		for (unsigned int i = 0; i < genalloc_len(stralist,&toreload) ; i++)
			newargv[m++] = gaistr(&toreload,i) ;
				
		newargv[m++] = 0 ;
			
		if (ssexec_dbctl(nargc,newargv,envp,info))
		{
			VERBO3 strerr_warnwu1x("bring down service list") ;
			goto err ;
		}
	}

	return 1 ;
	
	err:
		genalloc_deepfree(stralist,&toreload,stra_free) ;
		return 0 ;
}

int rc_start(ssexec_t *info,genalloc *ga,char const *const *envp)
{
	if (!db_switch_to(info,envp,SS_SWSRC))
		strerr_diefu3x(111,"switch: ",info->treename," to source") ;
	
	
	int nargc = 3 + genalloc_len(svstat_t,ga) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	if (RELOAD == 1) 
		newargv[m++] = "-r" ;
	else
		newargv[m++] = "-u" ;
		
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
		newargv[m++] = genalloc_s(svstat_t,ga)[i].name ; ;
		
	newargv[m++] = 0 ;
	
	if (ssexec_dbctl(nargc,newargv,envp,info))
	{
		VERBO3 strerr_warnwu1x("bring up service list") ;
		goto err ;
	}
	
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}

	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("delete reload resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"reload"))
		{
			VERBO3 strerr_warnwu3sys("delete resolve file",saresolve.s,"/reload") ;
			return 0 ;
		}
	}

	return 1 ;
	
	err: 
		return 0 ;
}

int ssexec_start(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	RELOAD = 0 ;
	DEADLINE = 0 ;
	saresolve = stralloc_zero ;
	SIG = "-U" ;
	DOWNFILE = genalloc_zero ; //stralist
	
	int r,rb ;
	
	stralloc svok = STRALLOC_ZERO ;
	
	genalloc nclassic = GENALLOC_ZERO ; //svstat_t type
	genalloc nrc = GENALLOC_ZERO ; //svstat_t type
	
	//PROG = "66-start" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">rR", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'r' : 	if (RELOAD) exitusage(usage_start) ; RELOAD = 1 ; SIG = "-R" ; break ;
				case 'R' : 	if (RELOAD) exitusage(usage_start) ; RELOAD = 2 ; SIG = "-R" ; break ;
				default : exitusage(usage_start) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) exitusage(usage_start) ;
	
	if (info->timeout) DEADLINE = info->timeout ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	if (!stralloc_cats(&svok,info->scandir.s)) retstralloc(111,"main") ; 
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
			svsrc.down = 0 ;
			svsrc.reload = 0 ;
			svsrc.init = 0 ;
			svok.len = svoklen ;
			
			if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
				strerr_diefu1x(111,"set revolve pointer to source") ;
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb < 0)
			{
				if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_BACK))
					strerr_diefu1x(111,"set revolve pointer to back") ;
				r = resolve_read(&type,saresolve.s,*argv,"type") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ; 
				if (r > 0) strerr_dief3x(111,"orphan service: ",*argv," :you can only stop it") ;
				if (r <= 0) strerr_dief2x(111,*argv,": is not enabled") ;
			}
			if (rb > 0)
			{
				svsrc.type = get_enumbyid(type.s,key_enum_el) ;
				svsrc.name = *argv ;
				svsrc.namelen = strlen(*argv) ;
				r = resolve_read(&type,saresolve.s,*argv,"reload") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ;
				if (r > 0) svsrc.reload = 1 ;
				r = resolve_read(&type,saresolve.s,*argv,"down") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ;
				if (r > 0) svsrc.down = 1 ;
				r = resolve_read(&type,saresolve.s,*argv,"remove") ;
				if (r > 0) strerr_dief3x(111,"service: ",*argv," was disabled, you can only stop it") ;
			}
			if (svsrc.type == CLASSIC)
			{
				
				if (!stralloc_cats(&svok,*argv)) retstralloc(111,"main") ; 
				if (!stralloc_0(&svok)) retstralloc(111,"main") ; 
				r = s6_svc_ok(svok.s) ;
				if (r < 0) strerr_diefu2sys(111,"check ", svok.s) ;
				if (!r)	svsrc.init = 1 ;
			}

			if (RELOAD > 1) svsrc.reload = 1 ;
			if (svsrc.init) svsrc.reload = 0 ;
			
			if (svsrc.type == CLASSIC)
			{
				if (!resolve_pointo(&saresolve,info,CLASSIC,SS_RESOLVE_SRC))
					strerr_diefu1x(111,"set revolve pointer to source") ;
				if (dir_search(saresolve.s,*argv,S_IFDIR) !=1) strerr_dief2x(111,*argv,": is not enabled") ;
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

	if (genalloc_len(svstat_t,&nclassic))
	{
		VERBO2 strerr_warni1x("sanitize svc services ...") ;
		if(!svc_sanitize(info,&nclassic,envp)) 
			strerr_diefu1x(111,"sanitize s6-svc services") ;
		VERBO2 strerr_warni1x("start svc services ...") ;
		if(!svc_start(info,&nclassic,envp)) 
			strerr_diefu1x(111,"start s6-svc services") ;
		VERBO2 strerr_warni3x("switch svc service of: ",info->treename," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch svc service of: ",info->treename," to source") ;
	} 
	if (genalloc_len(svstat_t,&nrc))
	{
		VERBO2 strerr_warni1x("sanitize rc services ...") ;
		if (!rc_sanitize(info,&nrc,envp)) 
			strerr_diefu1x(111,"sanitize s6-rc services") ;
		VERBO2 strerr_warni1x("start rc services ...") ;
		if (!rc_start(info,&nrc,envp)) 
			strerr_diefu1x(111,"update s6-rc services") ;
		VERBO2 strerr_warni3x("switch rc services of: ",info->treename," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename," to source") ;
	}
	
	stralloc_free(&saresolve) ;
	genalloc_free(svstat_t,&nclassic) ;
	genalloc_free(svstat_t,&nrc) ;
	genalloc_deepfree(stralist,&DOWNFILE,stra_free) ;
	
	return 0 ;		
}

	

