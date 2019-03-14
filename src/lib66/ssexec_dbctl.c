/* 
 * ssexec_dbctl.c
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
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>

#include <s6-rc/config.h>
#include <s6/s6-supervise.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/ssexec.h>

#include <stdio.h>

static unsigned int DEADLINE = 0 ;

static pid_t send(genalloc *gasv, char const *livetree, char const *signal,char const *const *envp)
{
	tain_t deadline ;
    tain_from_millisecs(&deadline, DEADLINE) ;
       
    tain_now_g() ;
    tain_add_g(&deadline, &deadline) ;

	char const *newargv[10 + genalloc_len(ss_resolve_t,gasv)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,DEADLINE)] = 0 ;
	
	newargv[m++] = S6RC_BINPREFIX "s6-rc" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-t" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = livetree ;
	newargv[m++] = signal ;
	newargv[m++] = "change" ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,gasv); i++)
		newargv[m++] = genalloc_s(ss_resolve_t,gasv)[i].sa.s + genalloc_s(ss_resolve_t,gasv)[i].name  ;
	
	newargv[m++] = 0 ;

	return child_spawn0(newargv[0],newargv,envp) ;
	
}

int ssexec_dbctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	DEADLINE = 0 ;
	
	if (info->timeout) DEADLINE = info->timeout ;

	unsigned int up, down, reload, ret ;
	
	int wstat, writein ;
	pid_t pid ;
	
	char *signal = 0 ;
	char *mainsv = "Master" ;
	
	genalloc resdeps = GENALLOC_ZERO ; //ss_resolve_t
	genalloc toreload = GENALLOC_ZERO ;//ss_resolve_t
	stralloc tmp = STRALLOC_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	up = down = reload = ret = 0 ;
	if (!access(info->tree.s,W_OK)) writein = SS_DOUBLE ;
	else writein = SS_SIMPLE ;
	
	
	//PROG = "66-dbctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "udr", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'u' :	up = 1 ; if (down || reload) exitusage(usage_dbctl) ; break ;
				case 'd' : 	down = 1 ; if (up || reload) exitusage(usage_dbctl) ; break ;
				case 'r' : 	reload = 1 ; if (down || up) exitusage(usage_dbctl) ; break ;
				default : exitusage(usage_dbctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (!up && !down && !reload){ strerr_warnw1x("signal must be set") ; exitusage(usage_dbctl) ; }
	
	if (down) signal = "-d" ;
	else signal = "-u" ;

	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) strerr_diefu1sys(111,"set revolve pointer to live") ;
	
	if (argc < 1)
	{
		unsigned int i = 0 ;
		genalloc tmp = GENALLOC_ZERO ;
		ss_resolve_t res = RESOLVE_ZERO ;
		if (!ss_resolve_check(sares.s,mainsv)) strerr_dief1sys(111,"inner bundle doesn't exit -- please make a bug report") ;
		if (!ss_resolve_read(&res,sares.s,mainsv)) strerr_diefu1sys(111,"read resolve file of inner bundle") ;
		if (res.ndeps)
		{
			if (!clean_val(&tmp,res.sa.s + res.deps)) strerr_dief1sys(111,"retrieve dependencies of inner bundle") ;
			for (;i < genalloc_len(stralist,&tmp) ; i++)
			{
				ss_resolve_t dres = RESOLVE_ZERO ;
				char *name = gaistr(&tmp,i) ;
				if (!ss_resolve_check(sares.s,name)) strerr_dief2sys(110,"unknow service: ",name) ;
				if (!ss_resolve_read(&dres,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
				if (!ss_resolve_append(&resdeps,&dres)) strerr_diefu1sys(111,"append resolve") ;
				if (reload) if (!ss_resolve_append(&toreload,&dres)) strerr_diefu1sys(111,"append resolve") ;
				ss_resolve_free(&dres) ;
			}
		}
		else
		{
			VERBO1 strerr_warni1x("nothing to do") ;
			ss_resolve_free(&res) ;
			genalloc_deepfree(stralist,&tmp,stra_free) ;
			goto freed ;
		}
		ss_resolve_free(&res) ;
		genalloc_deepfree(stralist,&tmp,stra_free) ;
	}
	else 
	{
		for(;*argv;argv++)
		{
			ss_resolve_t res = RESOLVE_ZERO ;
			char const *name = *argv ;
			if (!ss_resolve_check(sares.s,name)) strerr_dief2sys(110,"unknow service: ",name) ;
			if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (res.type == CLASSIC) strerr_dief2x(111,name," has type classic") ;
			
			if (up)
			{
				if (!ss_resolve_add_deps(&resdeps,&res,sares.s)) strerr_diefu2sys(111,"resolve dependencies of: ",name) ;
			}
			else 
			{ 
				if (!ss_resolve_add_rdeps(&resdeps,&res,sares.s)) strerr_diefu2sys(111,"resolve recursive dependencies of: ",name) ;
			}
			if (reload)
			{
				if (!ss_resolve_add_rdeps(&toreload,&res,sares.s)) strerr_diefu2sys(111,"resolve recursive dependencies of: ",name) ;
			}
			ss_resolve_free(&res) ;
		}
		
	}	
	if (!db_ok(info->livetree.s,info->treename.s))
		strerr_dief5sys(111,"db: ",info->livetree.s,"/",info->treename.s," is not running") ;

	if (!stralloc_cats(&tmp,info->livetree.s)) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,info->treename.s)) retstralloc(111,"main") ;
	if (!stralloc_0(&tmp)) retstralloc(111,"main") ;

	if (reload)
	{
		pid = send(&toreload,tmp.s,"-d",envp) ;
		
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu1sys(111,"wait for s6-rc") ;
		
		if (wstat) strerr_diefu1x(111," stop services list") ;
	}
		
	pid = send(&resdeps,tmp.s,signal,envp) ;
	
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu1sys(111,"wait for s6-rc") ;
	
	if (wstat) strerr_diefu2x(111,down ? " start " : " stop ","services list") ;
	
	genalloc_reverse(ss_resolve_t,&resdeps) ;
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&resdeps) ; i++)
	{ 
		int nret = 0 ;
		ss_resolve_t_ref pres = &genalloc_s(ss_resolve_t,&resdeps)[i] ;
		char *name = pres->sa.s + pres->name ;
		/** do not touch the Master resolve file*/
		if (obstr_equal(name,SS_MASTER + 1)) continue ;
		/** only check longrun service */
		if (pres->type == LONGRUN)
		{	
			if (!s6_svstatus_read(pres->sa.s + pres->runat,&status))
			{
				strerr_diefu4sys(111,"read status of: ",pres->sa.s + pres->runat," -- race condition, try 66-info -S ",pres->sa.s + pres->name) ;
			}
			else if (down)
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat) && status.pid)
				{
					VERBO1 strerr_warnwu2x("stop: ",name) ;
					ss_resolve_setflag(pres,SS_FLAGS_PID,(uint32_t)status.pid) ;
					nret = 1 ;
				}
				else ss_resolve_setflag(pres,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
			}
			else 
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat))
				{
					VERBO1 strerr_warnwu2x("start: ",name) ;
					nret = 1 ;
					ss_resolve_setflag(pres,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
				}
				else ss_resolve_setflag(pres,SS_FLAGS_PID,(uint32_t)status.pid) ;
			}
		}
		if (nret) ret = 111 ;
		ss_resolve_setflag(pres,SS_FLAGS_RUN,SS_FLAGS_TRUE) ;
		ss_resolve_setflag(pres,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(pres,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(pres,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
		VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
		if (!ss_resolve_write(pres,sares.s,name,writein))
		{
			VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
			ret = 111 ;
		}
		if (!nret) VERBO1 strerr_warni3x(reload ? "Reloaded" : up ? "Started" : "Stopped"," successfully: ",name) ;
	}
	
	freed:
	stralloc_free(&tmp) ;	
	stralloc_free(&sares) ;
	genalloc_deepfree(ss_resolve_t,&resdeps,ss_resolve_free) ;
	genalloc_deepfree(ss_resolve_t,&toreload,ss_resolve_free) ;
	
	return ret ;
}

	

