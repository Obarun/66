/* 
 * 66-start.c
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

#include <stdio.h>
unsigned int VERBOSITY = 1 ;

unsigned int RELOAD = 0 ;
static tain_t DEADLINE ;
stralloc saresolve = STRALLOC_ZERO ;

#define USAGE "66-start [ -h help ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -r reload ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-start <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: timeout\n"
"	-r: reload the service(s)\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int svc_shutnremove(char const *base,char const *scandir,char const *live,char const *tree,char const *treename, genalloc *ga, char const *const *envp)
{
	int r ;
	int wstat ;
	pid_t pid ; 
	
	size_t scanlen = strlen(scandir) ;
	
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
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
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
	
	char const *newargv[11 + genalloc_len(stralist,&tot)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,tain_to_millisecs(&DEADLINE))] = 0 ;

	newargv[m++] = SS_BINPREFIX "66-svctl" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	newargv[m++] = "-D" ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
		newargv[m++] = gaistr(&tot,i) ;
			
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu1x("shutdown list of services") ;
		return 0 ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
	{
		size_t namelen = gaistrlen(&tot,i) ;
		char const *name = gaistr(&tot,i)  ;
		char svscan[scanlen + 1 + namelen + 1] ;
		memcpy(svscan,scandir,scanlen) ;
		svscan[scanlen] = '/' ;
		memcpy(svscan + scanlen + 1,name, namelen) ;
		svscan[scanlen + 1 + namelen] = 0 ;
		
		VERBO3 strerr_warnt2x("remove: ", svscan) ;
		if (rm_rf(svscan) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",svscan) ;
			return 0 ;
		}
	}
	stralloc_free(&log) ;
	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
}
		
int svc_start(char const *base,char const *scandir,char const *live,char const *tree, char const *treename,genalloc *ga,char const *const *envp)
{
	int wstat ;
	pid_t pid ; 
	
	genalloc tot = genalloc_zero ; //stralist
	
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{	
		if (!stra_add(&tot,genalloc_s(svstat_t,ga)[i].name))
		{
			VERBO3 strerr_warnwu3x("add: ",genalloc_s(svstat_t,ga)[i].name," as service to start") ;
			return 0 ;
		}
		
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
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
	char const *newargv[11 + genalloc_len(stralist,&tot)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,tain_to_millisecs(&DEADLINE))] = 0 ;
	
	newargv[m++] = SS_BINPREFIX "66-svctl" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	newargv[m++] = "-U" ;
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
		newargv[m++] = gaistr(&tot,i) ;
	
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu1x("bring up services") ;
		return 0 ;
	}
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}

	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("remove reload resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"reload"))
		{
			VERBO3 strerr_warnwu3sys("remove resolve file",saresolve.s,"/reload") ;
			return 0 ;
		}
	}
	
	return 1 ;
}

int svc_init(char const *scandir,char const *src, genalloc *ga)
{
	
	int r ;
	gid_t gid = getgid() ;
	uint16_t id ;
		
	ftrigr_t fifo = FTRIGR_ZERO ;
	
	tain_now_g() ;
	tain_add_g(&DEADLINE, &DEADLINE) ;
	
	VERBO3 strerr_warnt1x("iniate fifo: fifo") ;
		if (!ftrigr_startf(&fifo, &DEADLINE, &STAMP))
			return 0 ;
		
	for (unsigned int i=0 ; i <genalloc_len(svstat_t,ga); i++) 
	{
		char const *name = genalloc_s(svstat_t,ga)[i].name ;
		size_t namelen = genalloc_s(svstat_t,ga)[i].namelen ;
			
		size_t srclen = strlen(src) ;	
		char svsrc[srclen + 1 + namelen + 1] ;
		memcpy(svsrc,src,srclen) ;
		svsrc[srclen] = '/' ;
		memcpy(svsrc + srclen + 1, name,namelen) ;
		svsrc[srclen + 1 + namelen] = 0 ;
		
		size_t svscanlen = strlen(scandir) ;
		char svscan[svscanlen + 1 + namelen + 6 + 1] ;
		memcpy(svscan,scandir,svscanlen) ;
		svscan[svscanlen] = '/' ;
		memcpy(svscan + svscanlen + 1, name,namelen) ;
		svscanlen = svscanlen + 1 + namelen ;
		svscan[svscanlen] = 0 ;
		
		VERBO3 strerr_warnt2x("init service: ", svscan) ;
			
		VERBO3 strerr_warnt4x("copy: ",svsrc, " to ", svscan) ;
		if (!hiercopy(svsrc,svscan ))
		{
			VERBO3 strerr_warnwu4sys("copy: ",svsrc," to: ",svscan) ;
			goto err ;
		}
		memcpy(svscan + svscanlen, "/down", 5) ;
		svscan[svscanlen + 5] = 0 ;
				
		VERBO3 strerr_warnt2x("create file: ",svscan) ;
		if (!touch(svscan))
		{
			VERBO3 strerr_warnwu2sys("create file: ",svscan) ;
			goto err ;
		}
		memcpy(svscan + svscanlen, "/event", 6) ;
		svscan[svscanlen + 6] = 0 ;	
		VERBO3 strerr_warnt2x("create fifo: ",svscan) ;
		if (!ftrigw_fifodir_make(svscan, gid, 0))
		{
			VERBO3 strerr_warnwu2sys("create fifo: ",svscan) ;
			goto err ;
		}
		VERBO3 strerr_warnt2x("subcribe to fifo: ",svscan) ;
		/** unsubscribe automatically, options is 0 */
		id = ftrigr_subscribe_g(&fifo, svscan, "s", 0, &DEADLINE) ;
		if (!id)
		{
			VERBO3 strerr_warnwu2x("subcribe to fifo: ",svscan) ;
			goto err ;
		}
	}
	VERBO3 strerr_warnt2x("reload scandir: ",scandir) ;
	r = s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		goto err ;
	}
	if (!r)
	{
		VERBO3 strerr_warnw3x("scandir: ",scandir, " is not running") ;
		goto err ;
	}
	
	VERBO3 strerr_warnt1x("waiting for events on fifo") ;
	if (ftrigr_wait_and_g(&fifo, &id, 1, &DEADLINE) < 0)
			goto err ;
	
	ftrigr_end(&fifo) ;
	
	return 1 ;
	
	err:
		ftrigr_end(&fifo) ;
		return 0 ;

}

int svc_sanitize(char const *base,char const *scandir,char const *live,char const *tree,char const *treename, genalloc *ga, char const *const *envp)
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
		if (!resolve_pointo(&saresolve,base,live,tree,treename,CLASSIC,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		VERBO3 strerr_warnt1x("initiate svc service list ...") ;
		if (!svc_init(scandir, saresolve.s, &toinit))
		{
			VERBO3 strerr_warnwu1x("iniatiate svc service list") ;
			return 0 ;
		}
	}
	if (genalloc_len(svstat_t,&toreload))
	{
		VERBO3 strerr_warnt1x("shutdown service before reload it ...") ;
		if (!svc_shutnremove(base,scandir,live,tree,treename,&toreload,envp))
		{
			VERBO3 strerr_warnwu1x("down list of service") ;
			return 0 ;
		}
		if (!resolve_pointo(&saresolve,base,live,tree,treename,CLASSIC,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		VERBO3 strerr_warnt1x("initiate svc services list ...") ;
		if (!svc_init(scandir, saresolve.s, &toreload))
		{
			VERBO3 strerr_warnwu1x("iniatiate svc service list") ;
			return 0 ;
		}
	}
	
	return 1 ;
}

int rc_init(char const *scandir, char const *live, char const *treename, char const *const *envp)
{
	pid_t pid ;
	int r, wstat ;
	
	char const *newargv[8] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
				
	newargv[m++] = SS_BINPREFIX "66-init" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-d" ;
	newargv[m++] = treename ;
	newargv[m++] = 0 ;
				
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	
	if (wstat)
	{
		strerr_warnwu2x("init rc services for tree: ",treename) ;
		return 0 ;
	}
	
	VERBO3 strerr_warnt2x("reload scandir: ",scandir) ;
	r = s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		return 0 ;
	}
	
	return 1 ;
}

int rc_sanitize(char const *base, char const *scandir, char const *live, char const *livetree, char const *tree, char const *treename,genalloc *ga, char const *const *envp,unsigned int trc)
{
	pid_t pid ;
	int wstat ;
	genalloc toreload = GENALLOC_ZERO ; //stralist
	
	size_t livetreelen = strlen(livetree) ;
	size_t namelen = strlen(treename) ;
	
	char db[livetreelen + 1 + namelen + 1] ;
	memcpy(db,livetree,livetreelen) ;
	db[livetreelen] = '/' ;
	memcpy(db + livetreelen + 1, treename, namelen) ;
	db[livetreelen + 1 + namelen] = 0 ;
	
	if (!db_ok(livetree, treename))
		if (!rc_init(scandir,live,treename,envp)) return 0 ;
	
		
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
		if (!db_switch_to(base,livetree,tree,treename,envp,SS_SWBACK))
		{
			VERBO3 strerr_warnwu3x("switch ",treename," to backup") ;
			return 0 ;
		}
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (!db_compile(saresolve.s,tree,treename,envp))
		{
			VERBO3 strerr_diefu4x(111,"compile ",saresolve.s,"/",treename) ;
			return 0 ;
		}
		if (!db_switch_to(base,livetree,tree,treename,envp,SS_SWSRC))
		{
			VERBO3 strerr_warnwu3x("switch ",treename," to backup") ;
			return 0 ;
		}
		char const *newargv[11 + genalloc_len(stralist,&toreload)] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;	

		int globalt ;
		tain_t globaltto ;
		tain_sub(&globaltto,&DEADLINE, &STAMP) ;
		globalt = tain_to_millisecs(&globaltto) ;
		if (!globalt) globalt = 1 ;
		if (globalt > 0 && (!trc || (unsigned int) globalt < trc))
			trc = (uint32_t)globalt ;
		
		char tt[UINT32_FMT] ;
		tt[uint32_fmt(tt,trc)] = 0 ;
	
		newargv[m++] = SS_BINPREFIX "66-dbctl" ;
		newargv[m++] = "-v" ;
		newargv[m++] = fmt ;
		newargv[m++] = "-T" ;
		newargv[m++] = tt ;
		newargv[m++] = "-l" ;
		newargv[m++] = live ;
		newargv[m++] = "-t" ;
		newargv[m++] = treename ;
		newargv[m++] = "-d" ;

		for (unsigned int i = 0; i < genalloc_len(stralist,&toreload) ; i++)
			newargv[m++] = gaistr(&toreload,i) ;
			
		newargv[m++] = 0 ;
		
		pid = child_spawn0(newargv[0],newargv,envp) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
		{
			VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
			goto err ;
		}
	
		if (wstat)
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
int rc_start(char const *base,char const *live, char const *livetree,char const *tree, char const *treename,genalloc *ga,char const *const *envp,unsigned int trc)
{

	int wstat ;
	pid_t pid ;
	
	if (!db_switch_to(base,livetree,tree,treename,envp,SS_SWSRC))
		strerr_diefu3x(111,"switch: ",treename," to source") ;
	
	char const *newargv[11 + genalloc_len(stralist,ga)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;	
	
	int globalt ;
	tain_t globaltto ;
	tain_sub(&globaltto,&DEADLINE, &STAMP) ;
	globalt = tain_to_millisecs(&globaltto) ;
	if (!globalt) globalt = 1 ;
	if (globalt > 0 && (!trc || (unsigned int) globalt < trc))
		trc = (uint32_t)globalt ;
	
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,trc)] = 0 ;
	
	newargv[m++] = SS_BINPREFIX "66-dbctl" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	newargv[m++] = "-u" ;

	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
		newargv[m++] = genalloc_s(svstat_t,ga)[i].name ; ;
		
	newargv[m++] = 0 ;
		
	tain_now_g() ;
	tain_add_g(&DEADLINE,&DEADLINE) ;
	
	int spfd = selfpipe_init() ;
	if (spfd < 0) VERBO3 strerr_diefu1sys(111,"init selfpipe") ;
	{
		sigset_t set ;
		sigemptyset(&set) ;
		sigaddset(&set, SIGCHLD) ;
		sigaddset(&set, SIGINT) ;
		sigaddset(&set, SIGTERM) ;
		if (selfpipe_trapset(&set) < 0)
			VERBO3 strerr_diefu1sys(111,"trap signals") ;
	}
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		goto err ;
	}

	if (wstat)
	{
		VERBO3 strerr_warnwu1x("bring up service list") ;
		goto err ;
	}
		
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}

	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("remove reload resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"reload"))
		{
			VERBO3 strerr_warnwu3sys("remove resolve file",saresolve.s,"/reload") ;
			return 0 ;
		}
	}

	return 1 ;
	
	err: 
		selfpipe_finish() ;
		return 0 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r,rb ;
	unsigned int trc ;
	
	uid_t owner ;
		
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc svok = STRALLOC_ZERO ;
	
	genalloc nclassic = GENALLOC_ZERO ; //svstat_t type
	genalloc nrc = GENALLOC_ZERO ; //svstat_t type
		
	unsigned int tmain = trc = 0 ;
	
	PROG = "66-start" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:rT:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;

			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'T' :	if (!uint0_scan(l.arg, &tmain)) exitusage() ; break ;
				case 'r' : 	RELOAD = 1 ; break ;
				case 'l' : 	if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 't' : 	if(!stralloc_cats(&tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&tree)) retstralloc(111,"main") ;
							break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage() ;
	
	owner = MYUID ;
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (tmain){
		tain_from_millisecs(&DEADLINE, tmain) ;
		trc = tmain ;
	}
	else DEADLINE = tain_infinite_relative ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	size_t treelen = get_rlen_until(tree.s,'/',tree.len - 1) ;
	size_t treenamelen = (tree.len - 1) - treelen ;
	char treename[treenamelen + 1] ;
	memcpy(treename, tree.s + treelen + 1,treenamelen) ;
	treenamelen-- ;
	treename[treenamelen] = 0 ;
	
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
	
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;

	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", scandir.s," is not running") ;
	
	if (!stralloc_catb(&svok,scandir.s,scandir.len - 1)) retstralloc(111,"main") ; 
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
			svsrc.reload = 0 ;
			svsrc.init = 0 ;
			svok.len = svoklen ;
			
			if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
				strerr_diefu1x(111,"set revolve pointer to source") ;
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb < 0)
			{
				if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_BACK))
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

			if (RELOAD) svsrc.reload = 1 ;
			if (svsrc.init) svsrc.reload = 0 ;
			
			if (svsrc.type == CLASSIC)
			{
				if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,CLASSIC,SS_RESOLVE_SRC))
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
		if(!svc_sanitize(base.s,scandir.s,live.s,tree.s,treename,&nclassic,envp)) 
			strerr_diefu1x(111,"sanitize s6-svc services") ;
		VERBO2 strerr_warni1x("start svc services ...") ;
		if(!svc_start(base.s,scandir.s,live.s,tree.s,treename,&nclassic,envp)) 
			strerr_diefu1x(111,"start s6-svc services") ;
		VERBO2 strerr_warni3x("switch svc service of: ",treename," to source") ;
		if (!svc_switch_to(base.s,tree.s,treename,SS_SWSRC))
			strerr_diefu3x(111,"switch svc service of: ",treename," to source") ;
	} 
	if (genalloc_len(svstat_t,&nrc))
	{
		VERBO2 strerr_warni1x("sanitize rc services ...") ;
		if (!rc_sanitize(base.s,scandir.s,live.s,livetree.s,tree.s,treename,&nrc,envp,trc)) 
			strerr_diefu1x(111,"sanitize s6-rc services") ;
		VERBO2 strerr_warni1x("start rc services ...") ;
		if (!rc_start(base.s,live.s,livetree.s,tree.s,treename,&nrc,envp,trc)) 
			strerr_diefu1x(111,"update s6-rc services") ;
		VERBO2 strerr_warni3x("switch rc services of: ",treename," to source") ;
		if (!db_switch_to(base.s,livetree.s,tree.s,treename,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",livetree.s,"/",treename," to source") ;
	}
	
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&scandir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	stralloc_free(&saresolve) ;
	genalloc_free(svstat_t,&nclassic) ;
	genalloc_free(svstat_t,&nrc) ;
	
	return 0 ;		
}
	

