/* 
 * ssexec_all.c
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
#include <sys/ioctl.h>
#include <fcntl.h>

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
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/tai.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/selfpipe.h>
#include <skalibs/sig.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/ssexec.h>

#include <s6/s6-supervise.h>
#include <s6/config.h>


#include <stdio.h>

static unsigned int DEADLINE = 0 ;

int doit(ssexec_t *info, unsigned int what, char const *const *envp)
{
	genalloc ga = GENALLOC_ZERO ; //stralist
	
	char src[info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1] ;
	memcpy(src,info->tree.s,info->tree.len) ;
	memcpy(src + info->tree.len, SS_SVDIRS,SS_SVDIRS_LEN) ;
	memcpy(src + info->tree.len +SS_SVDIRS_LEN, SS_SVC,SS_SVC_LEN) ;
	src[info->tree.len +SS_SVDIRS_LEN + SS_SVC_LEN] = 0 ;
	
	if (!dir_get(&ga,src,"",S_IFDIR))
	{
		VERBO3 strerr_warnwu2x("find source of classic service for tree: ",info->treename.s) ;
		return 0 ;
	}
	if (!genalloc_len(stralist,&ga))
	{
		VERBO3 strerr_warni4x("no classic service for tree: ",info->treename.s," to ", what ? "start" : "stop") ;
	}
	/** add transparent Master to start the db*/
	if (!stra_add(&ga,"Master"))
	{
		VERBO3 strerr_warnwu2x("add Master as service to ", what ? "start" : "stop") ;
		return 0 ;
	}
	
	int nargc = 2 + genalloc_len(stralist,&ga) ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
		
	newargv[m++] = "fake_name" ;
			
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&ga) ; i++)
		newargv[m++] = gaistr(&ga,i) ;
		
	newargv[m++] = 0 ;
	for (int i = 0 ; i < m; i++)
		printf("newarg::%s\n",newargv[i]) ;
	if (what)
	{
		if (ssexec_start(nargc,newargv,envp,info))
		{
			genalloc_deepfree(stralist,&ga,stra_free) ;
			return 0 ;
		}
	}
	else
	if (ssexec_stop(nargc,newargv,envp,info))
	{
		genalloc_deepfree(stralist,&ga,stra_free) ;
		return 0 ;
	}	
	
	genalloc_deepfree(stralist,&ga,stra_free) ;
	return 1 ;
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

int ssexec_all(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	DEADLINE = 0 ;

	int r ;
	int what ;
	int shut = 0 ;
	int fd ;
	int intree = 0 ;
	
	stralloc contents = STRALLOC_ZERO ;
	genalloc in = GENALLOC_ZERO ; //stralist
	
	what = 1 ;

	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">o:f", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'o' : 	if (!stralloc_obreplace(&info->treename,l.arg))
								strerr_diefu1sys(111,"keep treename") ;
							intree = 1 ; break ;
				case 'f' : 	shut = 1 ; break ;
				default : exitusage(usage_all) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc != 1) exitusage(usage_all) ; 
	
	if (*argv[0] == 'u') what = 1 ;
	else if (*argv[0] == 'd') what = 0 ;
	else exitusage(usage_all) ;
	
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ",info->scandir.s," is not running") ;
	
	size_t statesize ;
	/** /system/state */
	size_t statelen ;
	char state[info->base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
	memcpy(state,info->base.s,info->base.len) ;
	memcpy(state + info->base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(state + info->base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
	statelen = info->base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
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
			
	/** only one tree?*/
	if (intree)
	{
		if (!stra_add(&in,info->treename.s)) strerr_diefu3x(111,"add: ", info->treename.s," as tree to start") ;
	}
	else
	{
		if (!clean_val(&in,contents.s))
			strerr_diefu2x(111,"clean: ",contents.s) ;
	}
	
	if (!in.len)
	{
		strerr_warni1x("nothing to do") ;
		goto freed ;
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
		stralloc tree = STRALLOC_ZERO ;
		
		char *treename = gaistr(&in,i) ;
		
		if(!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
		if(!stralloc_0(&tree)) retstralloc(111,"main") ;
				
		r = tree_sethome(&tree,info->base.s,info->owner) ;
		if (r < 0 || !r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
		if (!stralloc_obreplace(&info->tree,tree.s)) strerr_diefu1sys(111,"replace info->treename string") ;
		
		stralloc_free(&tree) ;
		
		if (!tree_get_permissions(info->tree.s,info->owner))
			strerr_dief2x(110,"You're not allowed to use the tree: ",info->tree.s) ;
		
		if (!stralloc_obreplace(&info->treename,treename)) strerr_diefu1sys(111,"replace info->treename string") ;
		info->tree.len--;
		info->treename.len--;
	
		if (what)
		{
			int nargc = 4 ;
			char const *newargv[nargc] ;
			unsigned int m = 0 ;
		
			newargv[m++] = "fake_name" ;
			newargv[m++] = "-B" ;
			newargv[m++] = info->treename.s ;
			newargv[m++] = 0 ;
				
			if (ssexec_init(nargc,newargv,envp,info))
				strerr_diefu2x(111,"initiate services of tree: ",treename) ;
			
			r = s6_svc_writectl(info->scandir.s, S6_SVSCAN_CTLDIR, "an", 2) ;
			if (r < 0) strerr_dief3sys(111,"something is wrong with the ",info->scandir.s, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		
		}
			
		if (!doit(info,what,envp)) strerr_warnwu3x((what) ? "start" : "stop" , " service for tree: ",treename) ;
		
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
	
	freed:
		stralloc_free(&contents) ;
		genalloc_deepfree(stralist,&in,stra_free) ;
	
	return 0 ;
}

	

