/* 
 * 66-all.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/tree.h>

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
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

int doit(char const *tree,char const *treename,char const *live, unsigned int what,uid_t owner, char const *const *envp)
{
	int wstat ;
	pid_t pid ; 
	
	stralloc dirlist = STRALLOC_ZERO ;
	
	char ownerstr[UID_FMT] ;
	size_t ownerlen = uid_fmt(ownerstr,owner) ;
	ownerstr[ownerlen] = 0 ;
		   
	size_t livelen = strlen(live) - 1 ;
	size_t treenamelen = strlen(treename) ;
	char src[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + treenamelen + 1] ;
	memcpy(src,live,livelen) ;
	memcpy(src + livelen, SS_STATE,SS_STATE_LEN) ;
	src[livelen + SS_STATE_LEN] = '/' ;
	memcpy(src + livelen + SS_STATE_LEN + 1,ownerstr,ownerlen) ;
	src[livelen + SS_STATE_LEN + 1 + ownerlen] = '/' ;
	memcpy(src + livelen + SS_STATE_LEN + 1 + ownerlen + 1,treename,treenamelen) ;
	src[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + treenamelen] = 0 ;

    if (!sastr_dir_get(&dirlist,src,"init",S_IFREG))
		log_dieusys(LOG_EXIT_SYS,"get init file from directory: ",src) ;

	if (dirlist.len)
	{
		size_t i = 0, len = sastr_len(&dirlist) ;
		char const *newargv[10 + len + 1] ;
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
		
		len = dirlist.len ;
		for (;i < len; i += strlen(dirlist.s + i) + 1)
			newargv[m++] = dirlist.s+i ;
		
		newargv[m++] = 0 ;
		
		pid = child_spawn0(newargv[0],newargv,envp) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
		{
			log_warnusys("wait for ",newargv[0]) ;
			goto err ;
		}
		if (wstat) goto err ;
	}
	else log_info("Empty tree: ",treename," -- nothing to do") ;

	stralloc_free(&dirlist) ;
	return 1 ;
	err:
		stralloc_free(&dirlist) ;
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

	if (setsid() < 0) log_dieusys(LOG_EXIT_SYS,"setsid") ;
	if ((chdir("/")) < 0) log_dieusys(LOG_EXIT_SYS,"chdir") ;
	ioctl(0,TIOCSCTTY,1) ;

	umask(022) ;
}


int main(int argc, char const *const *argv,char const *const *envp)
{
	int r, what, wstat, shut = 0, fd ;
	pid_t pid ; 
	uid_t owner ;
	size_t statesize, statelen, pos = 0 ;
	char const *treename = NULL ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc contents = STRALLOC_ZERO ;
	stralloc ugly = STRALLOC_ZERO ;
	
	what = 1 ;
	
	PROG = "66-all" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:T:t:f", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) log_die_nomem("stralloc") ;
							if (!stralloc_0(&live)) log_die_nomem("stralloc") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &DEADLINE)) log_usage(USAGE) ; break ;
				case 't' : 	treename = l.arg ; break ;
				case 'f' : 	shut = 1 ; break ;
				default : 	log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc != 1) log_usage(USAGE) ;
	
	if (*argv[0] == 'u') what = 1 ;
	else if (*argv[0] == 'd') what = 0 ;
	else log_usage(USAGE) ;
	
	owner = MYUID ;

	if (!set_ownersysdir(&base,owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
	
	r = set_livedir(&live) ;
	if (!r) log_die_nomem("stralloc") ;
	if (r < 0 ) log_die(LOG_EXIT_SYS,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&scandir,&live))log_die_nomem("stralloc") ;
	
	r = set_livescan(&scandir,owner) ;
	if (!r) log_die_nomem("stralloc") ;
	if (r < 0 ) log_die(LOG_EXIT_SYS,"scandir: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) <= 0) log_die(LOG_EXIT_SYS,"scandir: ", scandir.s," is not running") ;
	
	if (!stralloc_copy(&livetree,&live)) log_die_nomem("stralloc") ;
	
	r = set_livetree(&livetree,owner) ;
	if (!r) log_die_nomem("stralloc") ;
	if (r < 0 ) log_die(LOG_EXIT_SYS,"livetree: ",livetree.s," must be an absolute path") ;
	
	char ste[base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
	memcpy(ste,base.s,base.len) ;
	memcpy(ste + base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(ste + base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
	statelen = base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
	ste[statelen] = 0 ;

	r = scan_mode(ste,S_IFREG) ;
	if (r < 0) log_die(LOG_EXIT_SYS,"conflict format for: ",ste) ;
	if (!r)	log_dieusys(LOG_EXIT_SYS,"find: ",ste) ;
	
	/** only one tree?*/
	if (treename)
	{
		if (!stralloc_cats(&contents,treename))log_dieu(LOG_EXIT_SYS,"add: ", treename," as tree to start") ;
		if (!stralloc_0(&contents)) log_die_nomem("stralloc") ;
	}
	else
	{
		statesize = file_get_size(ste) ;
	
		r = openreadfileclose(ste,&contents,statesize) ;
		if(!r) log_dieusys(LOG_EXIT_SYS,"open: ", ste) ;

		/** ensure that we have an empty line at the end of the string*/
		if (!stralloc_cats(&contents,"\n")) log_die_nomem("stralloc") ;
		if (!stralloc_0(&contents)) log_die_nomem("stralloc") ;
	
		if (!sastr_clean_element(&contents)) 
		{
			log_info("nothing to do") ;
			goto end ;
		}
		
	}
	
	if (shut)
	{
		pid_t dpid ;  
		int wstat = 0 ;
	
		dpid = fork() ;  
		
		if (dpid < 0) log_dieusys(LOG_EXIT_SYS,"fork") ;  
		else if (dpid > 0)
		{
			if (waitpid_nointr(dpid,&wstat, 0) < 0)
				log_dieusys(LOG_EXIT_SYS,"wait for child") ;

			if (wstat)
				log_die(LOG_EXIT_SYS,"child fail") ;
		
			goto end ;
			
		}
		else redir_fd() ;
	}
			
	for (;pos < contents.len; pos += strlen(contents.s + pos) + 1)
	{
		tree.len = 0 ;
		
		char *treename = contents.s + pos ;
		
		if(!stralloc_cats(&tree,treename)) log_die_nomem("stralloc") ;
		if(!stralloc_0(&tree)) log_die_nomem("stralloc") ;
		
		r = tree_sethome(&tree,base.s,owner) ;
		if (r < 0 || !r) log_dieusys(LOG_EXIT_SYS,"find tree: ", tree.s) ;
	
		if (!tree_get_permissions(tree.s,owner))
			log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",tree.s) ;
		/* we need to make an ugly check here, a tree might be empty.
		 * 66-init will not complain about this but doit function will do.
		 * So, we need to scan the tree before trying to start any service from it.
		 * This check is a temporary one because the tree dependencies feature
		 * will come on a near future and it will change this default behavior */
		size_t newlen = tree.len ;
		if (!stralloc_cats(&tree,SS_SVDIRS SS_SVC)) log_die_nomem("stralloc") ;
		if (!stralloc_0(&tree)) log_die_nomem("stralloc") ;
		
		if (!sastr_dir_get(&ugly,tree.s,"",S_IFDIR)) log_dieu(LOG_EXIT_SYS,"get classic services from: ",tree.s) ;
		if (!ugly.len)
		{
			tree.len = newlen ;
			ugly.len = 0 ;
			if (!stralloc_cats(&tree,SS_SVDIRS SS_DB SS_SRC)) log_die_nomem("stralloc") ;
			if (!stralloc_0(&tree)) log_die_nomem("stralloc") ;
			if (!sastr_dir_get(&ugly,tree.s,SS_MASTER+1,S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get rc services from: ",tree.s) ;
			if (!ugly.len)
			{
				log_info("empty tree: ",treename," -- nothing to do") ;
				goto end ;
			}
		}
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
				log_dieusys(LOG_EXIT_SYS,"wait for ",newargv[0]) ;
			
			if (wstat)
				log_dieu(LOG_EXIT_SYS,"initiate services of tree: ",treename) ;
			
			log_trace("reload scandir: ",scandir.s) ;
			if (scandir_send_signal(scandir.s,"an") <= 0) 
				log_dieusys(LOG_EXIT_SYS,"reload scandir: ",scandir.s) ;
				
		}
		
		if (!doit(tree.s,treename,live.s,what,owner,envp)) log_dieu(LOG_EXIT_SYS,(what) ? "start" : "stop" , " services of tree: ",treename) ;
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
		stralloc_free(&ugly) ;
			
	return 0 ;
}
