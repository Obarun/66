/* 
 * 66-all.c
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

#include <oblibs/error2.h>
#include <oblibs/obgetopt.h>
#include <oblibs/stralist.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/tai.h>
#include <skalibs/unix-transactional.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/tree.h>

#include <s6/s6-supervise.h>
#include <s6/config.h>

#include <stdio.h>
unsigned int VERBOSITY = 1 ;
static tain_t DEADLINE ;
unsigned int trc = 0 ;
#define USAGE "66-all [ -h help ] [ -v verbosity ] [ -T timeout ] [ -l live ] up/down"

static inline void info_help (void)
{
  static char const *help =
"66-all <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}
int get_fromdir(genalloc *ga,char const *srcdir)
{
	int fdsrc ;
	
	DIR *dir = opendir(srcdir) ;
	if (!dir)
		return 0 ;
	
	fdsrc = dir_fd(dir) ;
	
	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		if (stat_at(fdsrc, d->d_name, &st) < 0)
			return 0 ;
		if (S_ISDIR(st.st_mode))
			if (!stra_add(ga,d->d_name)) return 0 ;		 
	}

	return 1 ;
}
int doit(char const *tree,char const *treename,char const *live, unsigned int what, char const *const *envp)
{
	int wstat ;
	pid_t pid ; 
	size_t treelen = strlen(tree) ;
	genalloc ga = GENALLOC_ZERO ; //stralist
	
	char src[treelen + SS_SVDIRS_LEN + SS_SVC_LEN + 1] ;
	memcpy(src,tree,treelen) ;
	memcpy(src + treelen, SS_SVDIRS,SS_SVDIRS_LEN) ;
	memcpy(src + treelen +SS_SVDIRS_LEN, SS_SVC,SS_SVC_LEN) ;
	src[treelen +SS_SVDIRS_LEN + SS_SVC_LEN] = 0 ;
	
	if (!get_fromdir(&ga,src))
	{
		VERBO3 strerr_warnwu2x("find source of classic service for tree: ",treename) ;
		return 0 ;
	}
	if (!genalloc_len(stralist,&ga))
	{
		VERBO3 strerr_warni4x("no classic service for tree: ",treename," to ", what ? "start" : "stop") ;
	}
	if (!stra_add(&ga,"Master"))
	{
		VERBO3 strerr_warnwu2x("add Master as service to ", what ? "start" : "stop") ;
		return 0 ;
	}
	tain_now_g() ;
	tain_add_g(&DEADLINE, &DEADLINE) ;
	
	char const *newargv[10 + genalloc_len(stralist,&ga)] ;
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

	if (what)
		newargv[m++] = SS_BINPREFIX "66-start" ;
	else
		newargv[m++] = SS_BINPREFIX "66-stop" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&ga) ; i++) 
		newargv[m++] = gaistr(&ga,i) ;
			
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu3x(what ? "start" : "stop"," classic services for tree: ", treename) ;
		return 0 ;
	}
			
	return 1 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	int what ;
	int wstat ;
	pid_t pid ; 
	unsigned int tmain = 0 ;
	uid_t owner ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc contents = STRALLOC_ZERO ;
	genalloc in = GENALLOC_ZERO ; //stralist
	
	what = -1 ;
	
	PROG = "66-all" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:T:", &l) ;
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
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc != 1) exitusage() ;
	
	if (*argv[0] == 'u') what = 1 ;
	else if (*argv[0] == 'd') what = 0 ;
	else exitusage() ;
	
	if (tmain){
		tain_from_millisecs(&DEADLINE, tmain) ;
		trc = tmain ;
	}
	else DEADLINE = tain_infinite_relative ;;

	owner = MYUID ;

	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
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
	
	size_t statesize ;
	/** /system/state */
	base.len-- ;
	size_t statelen ;
	char state[base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
	memcpy(state,base.s,base.len) ;
	memcpy(state + base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(state + base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
	statelen = base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
	state[statelen] = 0 ;

	r = scan_mode(state,S_IFREG) ;
	if (r < 0) { errno = EEXIST ; return -1 ; }
	if (!r)	strerr_diefu2sys(111,"find: ",state) ;
	
	statesize = file_get_size(state) ;
	
	r = openreadfileclose(state,&contents,statesize) ;
	if(!r) strerr_diefu2sys(111,"open: ", state) ;

	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(&contents,"\n")) retstralloc(111,"main") ;
	if (!stralloc_0(&contents)) retstralloc(111,"main") ;
		
	if (!clean_val(&in,contents.s))
		strerr_diefu2x(111,"clean: ",contents.s) ;
	
	if (!in.len)
	{
		strerr_warni1x("nothing to do") ;
		return 0 ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&in) ; i++)
	{
		tree = stralloc_zero ;
		
		char *treename = gaistr(&in,i) ;
		
		if(!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
		if(!stralloc_0(&tree)) retstralloc(111,"main") ;
		
		r = tree_sethome(&tree,base.s) ;
		if (r < 0 || !r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
		if (!tree_get_permissions(tree.s))
			strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
		
		if (what)
		{
			char const *newargv[8] ;
			unsigned int m = 0 ;
			char fmt[UINT_FMT] ;
			fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
				
			newargv[m++] = SS_BINPREFIX "66-init" ;
			newargv[m++] = "-v" ;
			newargv[m++] = fmt ;
			newargv[m++] = "-l" ;
			newargv[m++] = live.s ;
			newargv[m++] = "-B" ;
			newargv[m++] = treename ;
			newargv[m++] = 0 ;
				
			pid = child_spawn0(newargv[0],newargv,envp) ;
			if (waitpid_nointr(pid,&wstat, 0) < 0)
				strerr_diefu2sys(111,"wait for ",newargv[0]) ;
			
			if (wstat)
				strerr_diefu2x(111,"init services for tree: ",treename) ;
			
			VERBO3 strerr_warnt2x("reload scandir: ",scandir.s) ;
			r = s6_svc_writectl(scandir.s, S6_SVSCAN_CTLDIR, "an", 2) ;
			if (r < 0)
			{
				VERBO3 strerr_warnw3sys("something is wrong with the ",scandir.s, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
				return -1 ;
			}
		}
		if (!doit(tree.s,treename,live.s,what,envp)) strerr_diefu2x(111,"start service for tree: ",treename) ;
	}
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&tree) ;
	stralloc_free(&livetree) ;
	stralloc_free(&scandir) ;
	stralloc_free(&contents) ;
	
	return 0 ;
}
	

