/* 
 * 66-dbctl.c
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

#include <stdio.h>
unsigned int VERBOSITY = 1 ;
static tain_t DEADLINE ;
stralloc saresolve = STRALLOC_ZERO ;

#define USAGE "66-dbctl [ -h help ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u up ] [ -d down ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-dbctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-u: bring up service in database of tree\n"
"	-d: bring down service in database of tree\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	unsigned int up, down, tmain, trc ;
	
	int wstat ;
	pid_t pid ;
	
	uid_t owner ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	
	genalloc gasv = GENALLOC_ZERO ; //stralist
	
	up = down = tmain = trc = 0 ;
		
	PROG = "66-dbctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "hv:l:t:udT:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &tmain)) exitusage() ; break ;
				case 't' : 	if(!stralloc_cats(&tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&tree)) retstralloc(111,"main") ;
							break ;
				case 'u' :	up = 1 ; if (down) exitusage() ; break ;
				case 'd' : 	down = 1 ; if (up) exitusage() ; break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) exitusage() ;
	
	if (tmain){
		tain_from_millisecs(&DEADLINE, tmain) ;
		trc = tmain ;
	}
	else DEADLINE = tain_infinite_relative ;

	owner = MYUID ;

	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
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
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
	
	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", scandir.s," is not running") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;
	
	if (!db_ok(livetree.s,treename))
		strerr_dief5sys(111,"db: ",livetree.s,"/",treename," is not running") ;

	for(;*argv;argv++)
		if (!stra_add(&gasv,*argv)) strerr_diefu3sys(111,"add: ",*argv," as service to handle") ;
	
	livetree.len-- ;
	if (!stralloc_cats(&livetree,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&livetree,treename)) retstralloc(111,"main") ;
	if (!stralloc_0(&livetree)) retstralloc(111,"main") ;
	
	char const *newargv[10 + genalloc_len(stralist,&gasv)] ;
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
	
	newargv[m++] = S6RC_BINPREFIX "s6-rc" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-t" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = livetree.s ;
	if (down)
	{
		newargv[m++] = "-d" ;
	}
	else
	{
		newargv[m++] = "-u" ;
	}
	newargv[m++] = "change" ;
	
	for (unsigned int i = 0 ; i<genalloc_len(stralist,&gasv); i++)
		newargv[m++] = gaistr(&gasv,i) ;
	
	newargv[m++] = 0 ;
	
	
	/** implementation of a sigpipe is needed here in case of
	 * INT signal */
	int spfd = selfpipe_init() ;
	if (spfd < 0) VERBO3 strerr_diefu1sys(111,"init selfpipe") ;
	{
		sigset_t set ;
		sigemptyset(&set) ;
		sigaddset(&set, SIGCHLD) ;
		sigaddset(&set, SIGINT) ;
		sigaddset(&set, SIGTERM) ;
		if (selfpipe_trapset(&set) < 0)
			strerr_diefu1sys(111,"trap signals") ;
	}
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu2sys(111,"wait for ",newargv[0]) ;
	
	if (wstat)
	{
		if (down)
			strerr_diefu1x(111,"bring down service list") ;
		else
			strerr_diefu1x(111,"bring up service list") ;
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
	int e = 0 ;
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	stralloc stat = STRALLOC_ZERO ;
	if (!stralloc_catb(&stat,livetree.s,livetree.len - 1)) retstralloc(0,"rc_start") ; 
	
	if (!stralloc_cats(&stat,SS_SVDIRS)) retstralloc(0,"rc_start") ; 
	if (!stralloc_cats(&stat,"/")) retstralloc(0,"rc_start") ; 
	size_t newlen = stat.len ;
	
	if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	stralloc type = STRALLOC_ZERO ;
		
	for (unsigned int i = 0; i < genalloc_len(stralist,&gasv) ; i++)
	{
		char *svname = gaistr(&gasv,i) ;
		if (resolve_read(&type,saresolve.s,svname,"type") <= 0)
			strerr_diefu2sys(111,"read type of: ",svname) ;
		
		//if (get_enumbyid(type.s,key_enum_el) == CLASSIC || get_enumbyid(type.s,key_enum_el) == LONGRUN)
		if (get_enumbyid(type.s,key_enum_el) == LONGRUN)
		{
			stat.len = newlen ;
			
			if (!stralloc_cats(&stat,svname)) retstralloc(111,"main") ; 
			if (!stralloc_0(&stat)) retstralloc(111,"main") ; 
			
			if (!s6_svstatus_read(stat.s,&status)) strerr_diefu2sys(111,"read status of: ",stat.s) ;
					
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
		
	selfpipe_finish() ;
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&tree) ;
	stralloc_free(&livetree) ;
	stralloc_free(&scandir) ;
	stralloc_free(&stat) ;
	stralloc_free(&type) ;
	stralloc_free(&saresolve) ;
	return e ;
}
	

