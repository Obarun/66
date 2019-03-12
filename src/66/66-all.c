/* 
 * 66-all.c
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#include <oblibs/error2.h>
#include <oblibs/obgetopt.h>
#include <oblibs/stralist.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/tree.h>

unsigned int VERBOSITY = 1 ;
static unsigned int DEADLINE = 0 ;
unsigned int trc = 0 ;
#define USAGE "66-all [ -h ] [ -v verbosity ] [ -f ] [ -T timeout ] [ -l live ] [ -t tree ] up/down"

static inline void info_help (void)
{
  static char const *help =
"66-all <options> up/down\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-f: fork the process\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
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
	
	if (!dir_get(&ga,src,"",S_IFDIR))
	{
		VERBO3 strerr_warnwu2x("find source of classic service for tree: ",treename) ;
		goto err ;
	}
	if (!genalloc_len(stralist,&ga))
	{
		VERBO3 strerr_warni4x("no classic service for tree: ",treename," to ", what ? "start" : "stop") ;
	}
	/** add transparent Master to start the db*/
	if (!stra_add(&ga,"Master"))
	{
		VERBO3 strerr_warnwu2x("add Master as service to ", what ? "start" : "stop") ;
		goto err ;
	}
	
	{
		char const *newargv[10 + genalloc_len(stralist,&ga)] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
			
		char tt[UINT32_FMT] ;
		tt[uint32_fmt(tt,DEADLINE)] = 0 ;
		
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
			goto err ;
		}
		if (wstat) goto err ;
	}
	genalloc_deepfree(stralist,&ga,stra_free) ;	
	return 1 ;
	err:
		genalloc_deepfree(stralist,&ga,stra_free) ;	
		return 0 ;
}

static void redir_fd(void)  
{
	int fd ;
	while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0)
	{
		if (fd >= 3) break ;
	}
	dup2 (fd,0) ;
	dup2 (fd,1) ;
	dup2 (fd,2) ;
	fd_close(fd) ;

	if (setsid() < 0) strerr_diefu1sys(111,"setsid") ;
	if ((chdir("/")) < 0) strerr_diefu1sys(111,"chdir") ;
	ioctl(0,TIOCSCTTY,1) ;

	umask(022) ;
}


int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	int what ;
	int wstat ;
	int shut = 0 ;
	pid_t pid ; 
	uid_t owner ;
	int fd ;
	
	char const *treename = NULL ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc contents = STRALLOC_ZERO ;
	genalloc in = GENALLOC_ZERO ; //stralist
	
	what = 1 ;
	
	PROG = "66-all" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:T:t:f", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &DEADLINE)) exitusage(USAGE) ; break ;
				case 't' : 	treename = l.arg ; break ;
				case 'f' : 	shut = 1 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc != 1) exitusage(USAGE) ;
	
	if (*argv[0] == 'u') what = 1 ;
	else if (*argv[0] == 'd') what = 0 ;
	else exitusage(USAGE) ;
	
	owner = MYUID ;

	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
	
	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"scandir: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", scandir.s," is not running") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"livetree: ",livetree.s," must be an absolute path") ;
	
	size_t statesize ;
	/** /system/state */
	size_t statelen ;
	char state[base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
	memcpy(state,base.s,base.len) ;
	memcpy(state + base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(state + base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
	statelen = base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
	state[statelen] = 0 ;

	r = scan_mode(state,S_IFREG) ;
	if (r < 0) strerr_dief2x(111,"conflict format for: ",state) ;
	if (!r)	strerr_diefu2sys(111,"find: ",state) ;
	
	statesize = file_get_size(state) ;
	
	r = openreadfileclose(state,&contents,statesize) ;
	if(!r) strerr_diefu2sys(111,"open: ", state) ;

	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(&contents,"\n")) retstralloc(111,"main") ;
	if (!stralloc_0(&contents)) retstralloc(111,"main") ;
	
	/** only one tree?*/
	if (treename)
	{
		if (!stra_add(&in,treename)) strerr_diefu3x(111,"add: ", treename," as tree to start") ;
	}
	else
	{
		if (!clean_val(&in,contents.s))
			strerr_diefu2x(111,"clean: ",contents.s) ;
	}

	if (!in.len)
	{
		strerr_warni1x("nothing to do") ;
		goto end ;
	}
	
	if (shut)
	{
		pid_t dpid ;  
		int wstat = 0 ;
	
		dpid = fork() ;  
		
		if (dpid < 0) strerr_diefu1sys(111,"fork") ;  
		else if (dpid > 0)
		{
			if (waitpid_nointr(dpid,&wstat, 0) < 0)
				strerr_diefu1sys(111,"wait for child") ;

			if (wstat)
				strerr_dief1x(111,"child fail") ;
		
			goto end ;
			
		}
		else redir_fd() ;
	}
			
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&in) ; i++)
	{
		tree.len = 0 ;
		
		char *treename = gaistr(&in,i) ;
		
		if(!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
		if(!stralloc_0(&tree)) retstralloc(111,"main") ;
		
		r = tree_sethome(&tree,base.s,owner) ;
		if (r < 0 || !r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
		if (!tree_get_permissions(tree.s,owner))
			strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
		
		if (what)
		{
			char const *newargv[9] ;
			unsigned int m = 0 ;
			char fmt[UINT_FMT] ;
			fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
				
			newargv[m++] = SS_BINPREFIX "66-init" ;
			newargv[m++] = "-v" ;
			newargv[m++] = fmt ;
			newargv[m++] = "-l" ;
			newargv[m++] = live.s ;
			newargv[m++] = "-t" ;
			newargv[m++] = treename ;
			newargv[m++] = "b" ;
			newargv[m++] = 0 ;
			
			pid = child_spawn0(newargv[0],newargv,envp) ;
			if (waitpid_nointr(pid,&wstat, 0) < 0)
				strerr_diefu2sys(111,"wait for ",newargv[0]) ;
			
			if (wstat)
				strerr_diefu2x(111,"initiate services of tree: ",treename) ;
			
			VERBO3 strerr_warnt2x("reload scandir: ",scandir.s) ;
			if (scandir_send_signal(scandir.s,"an") <= 0) 
				strerr_diefu2sys(111,"reload scandir: ",scandir.s) ;
				
		}
		
		if (!doit(tree.s,treename,live.s,what,envp)) strerr_diefu3x(111,(what) ? "start" : "stop" , " service for tree: ",treename) ;
	}
	end:
		if (shut)
		{
			while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0)
			{
				if (fd >= 3) break ;
			}
			dup2 (fd,0) ;
			dup2 (fd,1) ;
			dup2 (fd,2) ;
			fd_close(fd) ;
		}
		stralloc_free(&base) ;
		stralloc_free(&live) ;
		stralloc_free(&tree) ;
		stralloc_free(&livetree) ;
		stralloc_free(&scandir) ;
		stralloc_free(&contents) ;
		genalloc_deepfree(stralist,&in,stra_free) ;
	
	return 0 ;
}
