/* 
 * ssexec_dbctl.c
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

//#include <stdio.h>

static unsigned int DEADLINE = 0 ;

static pid_t send(genalloc *gasv, char const *livetree, char const *signal,char const *const *envp)
{
	tain_t deadline ;
    tain_from_millisecs(&deadline, DEADLINE) ;
       
    tain_now_g() ;
    tain_add_g(&deadline, &deadline) ;

	char const *newargv[10 + genalloc_len(stralist,gasv)] ;
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
	
	for (unsigned int i = 0 ; i<genalloc_len(stralist,gasv); i++)
		newargv[m++] = gaistr(gasv,i) ;
	
	newargv[m++] = 0 ;

	return child_spawn0(newargv[0],newargv,envp) ;
	
}

int ssexec_dbctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	DEADLINE = 0 ;

	unsigned int up, down, reload ;
	
	int wstat ;
	pid_t pid ;
	
	char *signal = 0 ;
	char *mainsv = "Master" ;
	
	genalloc gasv = GENALLOC_ZERO ; //stralist
	stralloc tmp = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	stralloc src = STRALLOC_ZERO ;
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	up = down = reload = 0 ;
	
	if (!ss_resolve_pointo(&src,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
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
	if (argc < 1)
	{
		if (!stra_add(&gasv,mainsv)) strerr_diefu1sys(111,"add: Master as service to handle") ;
	}
	else 
	{
		
		for(;*argv;argv++)
		{
			char const *name = *argv ;
			res.sa.len = 0 ;
			if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2sys(110,"unknow service: ",name) ;
			if (!ss_resolve_read(&res,src.s,name)) strerr_diefu3sys(111,"read resolve file of: ",src.s,name) ;
			if (res.type == CLASSIC) strerr_dief2x(111,name," has type classic") ;
			if (!stra_add(&gasv,name)) strerr_diefu3sys(111,"add: ",name," as service to handle") ;
		}
		
	}
	if (info->timeout) DEADLINE = info->timeout ;
	
	if (!db_ok(info->livetree.s,info->treename.s))
		strerr_dief5sys(111,"db: ",info->livetree.s,"/",info->treename.s," is not running") ;

	if (!stralloc_cats(&tmp,info->livetree.s)) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,info->treename.s)) retstralloc(111,"main") ;
	if (!stralloc_0(&tmp)) retstralloc(111,"main") ;

	if (reload)
	{
		pid = send(&gasv,tmp.s,"-d",envp) ;
		
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu1sys(111,"wait for s6-rc") ;
		
		if (wstat) strerr_diefu3x(111,"bring",down ? " down " : " up ","services list") ;
	}
	
	if (down) signal = "-d" ;
	else signal = "-u" ;
	
	pid = send(&gasv,tmp.s,signal,envp) ;
	
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu1sys(111,"wait for s6-rc") ;
	
	if (wstat) strerr_diefu3x(111,"bring",down ? " down " : " up ","services list") ;
	
	/** we are forced to do this ugly check cause of the design
	 * of s6-rc(generally s6-svc) which is launch and forgot. So
	 * s6-rc will not warn us if the daemon fail when we don't use
	 * readiness which is rarely used on DESKTOP configuration due of
	 * the bad design of the majority of daemon.
	 * The result of the check is not guaranted due of the rapidity of the code.
	 * between the end of the s6-rc process and the check of the daemon status,
	 * the real value of the status can be not written yet,so we can hit
	 * this window.*/
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&gasv) ; i++)
	{
		res.sa.len = 0 ;
		char *name = gaistr(&gasv,i) ;
		if (obstr_equal(name,SS_MASTER + 1)) continue ;
		
		if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2sys(111,"unknow service: ",name) ;
		if (!ss_resolve_read(&res,src.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		/** only check longrun service */
		if (res.type == LONGRUN)
		{	
			if (!s6_svstatus_read(res.sa.s + res.runat,&status))
			{
				strerr_diefu4sys(111,"read status of: ",res.sa.s + res.runat," -- race condition, try 66-info -S ",res.sa.s + res.name) ;
			}
			if (down)
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat) && status.pid)
					strerr_diefu2x(111,"stop: ",name) ;
			}
			else if (up)
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat))
					strerr_diefu2x(111,"start: ",name) ;
			}
		}
		VERBO1 strerr_warni3x(name,down ? " stopped " : " started ", "successfully") ;
	}
	freed:
	ss_resolve_free(&res) ;
	stralloc_free(&tmp) ;	
	stralloc_free(&src) ;
	genalloc_deepfree(stralist,&gasv,stra_free) ;
	
	return 0 ;
}

	

