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

//#include <stdio.h>

static unsigned int DEADLINE = 0 ;
static stralloc saresolve = STRALLOC_ZERO ;

static pid_t send(genalloc *gasv, char const *livetree, char const *signal,char const *const *envp)
{
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
	saresolve = stralloc_zero ;

	unsigned int up, down, reload ;
	
	int wstat ;
	pid_t pid ;
	
	char *signal = 0 ;
	char *mainsv = "Master" ;
	
	genalloc gasv = GENALLOC_ZERO ; //stralist
	stralloc tmp = STRALLOC_ZERO ;
	
	up = down = reload = 0 ;
	
	//PROG = "66-dbctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">udr", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'u' :	up = 1 ; if (down) exitusage(usage_dbctl) ; break ;
				case 'd' : 	down = 1 ; if (up) exitusage(usage_dbctl) ; break ;
				case 'r' : 	reload = 1 ; if (down || up) exitusage(usage_dbctl) ; break ;
				default : exitusage(usage_dbctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) if (!stra_add(&gasv,mainsv)) strerr_diefu1sys(111,"add: Master as service to handle") ;
	
	if (info->timeout) DEADLINE = info->timeout ;
	
	if (!db_ok(info->livetree.s,info->treename))
		strerr_dief5sys(111,"db: ",info->livetree.s,"/",info->treename," is not running") ;

	for(;*argv;argv++)
		if (!stra_add(&gasv,*argv)) strerr_diefu3sys(111,"add: ",*argv," as service to handle") ;
	
	if (!stralloc_cats(&tmp,info->livetree.s)) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,info->treename)) retstralloc(111,"main") ;
	if (!stralloc_0(&tmp)) retstralloc(111,"main") ;
	
	if (reload)
	{
		pid = send(&gasv,tmp.s,"-d",envp) ;
		
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu1sys(111,"wait for s6-rc") ;
		
		if (wstat)
		{
			if (down)
				strerr_diefu1x(111,"bring down services list") ;
			else
				strerr_diefu1x(111,"bring up services list") ;
		}
	}
	
	if (down) signal = "-d" ;
	else signal = "-u" ;
	
	pid = send(&gasv,tmp.s,signal,envp) ;
	
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu1sys(111,"wait for s6-rc") ;
	
	if (wstat)
	{
		if (down)
			strerr_diefu1x(111,"bring down services list") ;
		else
			strerr_diefu1x(111,"bring up services list") ;
	}
	/** we are forced to do this ugly check cause of the design
	 * of s6-rc(generally s6-svc) which is launch and forgot. So
	 * s6-rc will not warn us if the daemon fail when we don't use
	 * readiness which is rarely used on DESKTOP configuration due of
	 * the bad design of the majority of daemon.
	 * The result of the check is not guaranted due of the rapidity of the code.
	 * between the end of the s6-rc process and the check of the daemon status,
	 * the real value of the status can be not written yet,so we can hit
	 * this window.*/
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	tmp.len-- ;
	if (!stralloc_cats(&tmp,SS_SVDIRS)) retstralloc(111,"main") ; 
	if (!stralloc_cats(&tmp,"/")) retstralloc(111,"main") ; 
	size_t newlen = tmp.len ;
	
	if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		strerr_diefu1x(111,"set revolve pointer to source") ;
	
	stralloc type = STRALLOC_ZERO ;
		
	for (unsigned int i = 0; i < genalloc_len(stralist,&gasv) ; i++)
	{
		char *svname = gaistr(&gasv,i) ;
		if (resolve_read(&type,saresolve.s,svname,"type") <= 0)
			strerr_diefu2sys(111,"read type of: ",svname) ;
		
		if (get_enumbyid(type.s,key_enum_el) == LONGRUN)
		{
			tmp.len = newlen ;
			
			if (!stralloc_cats(&tmp,svname)) retstralloc(111,"main") ; 
			
			if (!s6_svstatus_read(tmp.s,&status)) strerr_diefu2sys(111,"read status of: ",tmp.s) ;
					
			if (down)
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat) && status.pid)
					strerr_diefu2x(111,"stop: ",svname) ;
			}
			if (up)
			{
				if (WEXITSTATUS(status.wstat) && WIFEXITED(status.wstat))
					strerr_diefu2x(111,"start: ",svname) ;
			}
		}
	}
	
	stralloc_free(&tmp) ;	
	stralloc_free(&type) ;
	stralloc_free(&saresolve) ;
	
	return 0 ;
}

	

