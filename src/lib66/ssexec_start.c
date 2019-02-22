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
#include <66/resolve.h>

#include <stdio.h>

static unsigned int RELOAD = 0 ;
static unsigned int DEADLINE = 0 ;
static char *SIG = "-U" ;
static stralloc sares = STRALLOC_ZERO ;
static genalloc nclassic = GENALLOC_ZERO ; //resolve_t type
static genalloc nrc = GENALLOC_ZERO ; //resolve_t type

int svc_send(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp)
{
	unsigned int i = 0 ;
	int nargc = 3 + genalloc_len(ss_resolve_t,ga) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	newargv[m++] = sig ;
	
	for (; i < genalloc_len(ss_resolve_t,ga) ; i++) 
		newargv[m++] = genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].name ;
	
	newargv[m++] = 0 ;

	if (ssexec_svctl(nargc,newargv,envp,info))
		return 0 ;
	
	return 1 ;
}

int svc_shutnremove(ssexec_t *info, genalloc *ga, char const *sig,char const *const *envp)
{
	if (!svc_send(info,ga,sig,envp))
	{
		VERBO1 strerr_warnwu1x("shutdown list of services") ;
		return 0 ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++) 
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		VERBO1 strerr_warnt2x("Delete: ", string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
		if (rm_rf(string + genalloc_s(ss_resolve_t,ga)[i].runat) < 0)
		{
			VERBO1 strerr_warnwu2sys("delete: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
			return 0 ;
		}
	}
		
	return 1 ;
}

int svc_start(ssexec_t *info,genalloc *ga,char const *const *envp)
{	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	/** reverse to start first the logger */
	genalloc_reverse(ss_resolve_t,ga) ;
	if (!svc_send(info,ga,SIG,envp))
	{
		VERBO1 strerr_warnwu1x("bring up services") ;
		return 0 ;
	}
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,ga)[i].name ;
		genalloc_s(ss_resolve_t,ga)[i].reload = 0 ;
		genalloc_s(ss_resolve_t,ga)[i].init = 0 ;
		genalloc_s(ss_resolve_t,ga)[i].run = 1 ;
		genalloc_s(ss_resolve_t,ga)[i].unsupervise = 0 ;
		if (!s6_svstatus_read(string + genalloc_s(ss_resolve_t,ga)[i].runat,&status))
		{ 
			VERBO3 strerr_warnwu2sys("read status of: ",name) ;
			return 0 ;
		}
		genalloc_s(ss_resolve_t,ga)[i].pid = status.pid ;
		VERBO2 strerr_warnt2x("Write resolve file of: ",name) ;
		if (!ss_resolve_write(&genalloc_s(ss_resolve_t,ga)[i],sares.s,name))
		{
			VERBO3 strerr_warnwu2sys("write resolve file of: ",name) ;
			return 0 ;
		}
		if (genalloc_s(ss_resolve_t,ga)[i].logger)
		{
			VERBO2 strerr_warnt2x("Write logger resolve file of: ",name) ;
			if (!ss_resolve_setlognwrite(&genalloc_s(ss_resolve_t,ga)[i],sares.s))
			{
				VERBO2 strerr_warnwu2sys("write logger resolve file of: ",name) ;
				return 0 ;
			}
		}
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
		if (!svc_shutnremove(info,&toreload,"-D",envp))
		{
			VERBO1 strerr_warnwu1x("down list of service") ;
			goto err ;
		}
		genalloc_cat(ss_resolve_t,&toinit,&toreload) ;
	}
	if (!ss_resolve_pointo(&sares,info,CLASSIC,SS_RESOLVE_SRC))		
	{
		VERBO1 strerr_warnwu1x("set revolve pointer to source") ;
		goto err;
	}
	if (genalloc_len(ss_resolve_t,&toinit))
	{
		/** reverse to start first the logger */
		genalloc_reverse(ss_resolve_t,&toinit) ;
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

int rc_init(ssexec_t *info, char const *const *envp)
{
	int r ;
	
	int nargc = 4 ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	newargv[m++] = "-d" ;
	newargv[m++] = info->treename.s ;
	newargv[m++] = 0 ;
				
	if (ssexec_init(nargc,newargv,envp,info))
	{
		VERBO1 strerr_warnwu2x("init rc services for tree: ",info->treename.s) ;
		return 0 ;
	}
	
	VERBO1 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
	r = s6_svc_writectl(info->scandir.s, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO1 strerr_warnw3sys("something is wrong with the ",info->scandir.s, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		return 0 ;
	}
	
	return 1 ;
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
		if (!rc_init(info,envp)) return 0 ;
	

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
	if (genalloc_len(stralist,&toreload))
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
			VERBO1 strerr_warnwu3x("switch ",info->treename.s," to backup") ;
			goto err ;
		}
		
		int nargc = 3 + genalloc_len(ss_resolve_t,&toreload) ;
		char const *newargv[nargc] ;
		unsigned int m = 0 ;
		
		newargv[m++] = "fake_name" ;
		newargv[m++] = "-d" ;

		for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,&toreload) ; i++)
			newargv[m++] = genalloc_s(ss_resolve_t,&toreload)[i].sa.s + genalloc_s(ss_resolve_t,&toreload)[i].name ;
				
		newargv[m++] = 0 ;
			
		if (ssexec_dbctl(nargc,newargv,envp,info))
		{
			VERBO1 strerr_warnwu1x("bring down service list") ;
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
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	int r = db_find_compiled_state(info->livetree.s,info->treename.s) ;
	if (r)
	{
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu3x(111,"switch: ",info->treename.s," to source") ;
	}
	/** reverse to start first the logger */
	genalloc_reverse(ss_resolve_t,ga) ;
	
	int nargc = 3 + genalloc_len(ss_resolve_t,ga) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	if (RELOAD == 1) 
		newargv[m++] = "-r" ;
	else
		newargv[m++] = "-u" ;
		
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
		newargv[m++] = genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].name ;

	newargv[m++] = 0 ;
	
	if (ssexec_dbctl(nargc,newargv,envp,info))
	{
		VERBO3 strerr_warnwu1x("bring up service list") ;
		return 0 ;
	}
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		VERBO1 strerr_warnwu1sys("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,ga)[i].name ;
		genalloc_s(ss_resolve_t,ga)[i].reload = 0 ;
		genalloc_s(ss_resolve_t,ga)[i].init = 0 ;
		genalloc_s(ss_resolve_t,ga)[i].run = 1 ;
		genalloc_s(ss_resolve_t,ga)[i].unsupervise = 0 ;
		
		/** only check longrun */
		if (genalloc_s(ss_resolve_t,ga)[i].type == LONGRUN)
		{
			if (!s6_svstatus_read(string + genalloc_s(ss_resolve_t,ga)[i].runat,&status))
			{ 
				VERBO3 strerr_warnwu2sys("read status of: ",name) ;
				return 0 ;
			}
			genalloc_s(ss_resolve_t,ga)[i].pid = status.pid ;
		}
		VERBO2 strerr_warnt2x("Write resolve file of: ",name) ;
		if (!ss_resolve_write(&genalloc_s(ss_resolve_t,ga)[i],sares.s,name))
		{
			VERBO3 strerr_warnwu2sys("write resolve file of: ",name) ;
			return 0 ;
		}
		if (genalloc_s(ss_resolve_t,ga)[i].logger) 
		{
			VERBO2 strerr_warnt2x("Write logger resolve file of: ",name) ;
			if (!ss_resolve_setlognwrite(&genalloc_s(ss_resolve_t,ga)[i],sares.s))
			{
				VERBO2 strerr_warnwu2sys("write logger resolve file of: ",name) ;
				return 0 ;
			}
		}
	}
	
	
	return 1 ;
}

int ssexec_start(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	RELOAD = 0 ;
	DEADLINE = 0 ;
	SIG = "-U" ;
	sares.len = 0 ;
	
	ss_resolve_t_ref pres ;
		
	int cl, rc, logname ;
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
	
	if (info->timeout) DEADLINE = info->timeout ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	
	{
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
		
		for(;*argv;argv++)
		{
			char const *name = *argv ;
			logname = 0 ;
			printf("name::%s\n",name) ;
			ss_resolve_t res = RESOLVE_ZERO ;
			pres = &res ;
			if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2x(111,name,": is not enabled") ;
			if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (!res.disen && res.run)
			{
				 VERBO1 strerr_warnw3x("service: ",name," was disabled, you can only stop it") ;
				 continue ;
			}
			else if (!res.disen) strerr_dief2x(111,name,": is not enabled") ;
			
			logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
			if ((RELOAD > 1) && (logname > 0)) strerr_dief1x(111,"-R signal is not allowed to a logger") ;
			if (RELOAD > 1) res.reload = 1 ;
			if (res.init) res.reload = 0 ;
			
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
	if (cl)
	{
		VERBO1 strerr_warni1x("sanitize classic services ...") ;
		if(!svc_sanitize(info,&nclassic,envp)) 
			strerr_diefu1x(111,"sanitize classic services") ;
		VERBO1 strerr_warni1x("start classic services ...") ;
		if(!svc_start(info,&nclassic,envp)) 
			strerr_diefu1x(111,"start classic services") ;
		VERBO1 strerr_warni3x("switch classic service of: ",info->treename.s," to source") ;
		if (!svc_switch_to(info,SS_SWSRC))
			strerr_diefu3x(111,"switch classic service of: ",info->treename.s," to source") ;
	} 
	if (rc)
	{
		VERBO1 strerr_warni1x("sanitize atomic services ...") ;
		if (!rc_sanitize(info,&nrc,envp)) 
			strerr_diefu1x(111,"sanitize atomic services") ;
		VERBO1 strerr_warni1x("start atomic services ...") ;
		if (!rc_start(info,&nrc,envp)) 
			strerr_diefu1x(111,"update atomic services") ;
		VERBO1 strerr_warni3x("switch atomic services of: ",info->treename.s," to source") ;
		if (!db_switch_to(info,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",info->livetree.s,"/",info->treename.s," to source") ;
	}
	
	return 0 ;		
}
