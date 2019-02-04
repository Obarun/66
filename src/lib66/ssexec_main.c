/* 
 * ssexec_main.c
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

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>

#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/tree.h>

#include <stdio.h>

int set_ssinfo(ssexec_t *info)
{
	int r ;
	
	info->owner = MYUID ;
	
	if (!set_ownersysdir(&info->base,info->owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = tree_sethome(&info->tree,info->base.s,info->owner) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", info->tree.s) ;
	
	info->treename = tree_setname(info->tree.s) ;
	if (!info->treename) strerr_diefu1x(111,"set the tree name") ;
	
	if (!tree_get_permissions(info->tree.s,info->owner))
		strerr_dief2x(110,"You're not allowed to use the tree: ",info->tree.s) ;
	else info->treeallow = 1 ;

	r = set_livedir(&info->live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",info->live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&info->livetree,&info->live)) retstralloc(111,"main") ;
	if (!stralloc_copy(&info->scandir,&info->live)) retstralloc(111,"main") ;
	
	r = set_livetree(&info->livetree,info->owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"livetree: ",info->livetree.s," must be an absolute path") ;
	
	r = set_livescan(&info->scandir,info->owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"scandir: ",info->scandir.s," must be an absolute path") ;
	
	return 0 ;
}

static inline void info_help (char const *help)
{
	if (buffer_putsflush(buffer_1, help) < 0)
		strerr_diefu1sys(111, "write to stdout") ;
}



int ssexec_main(int argc, char const *const *argv,char const *const *envp,ssexec_func_t *func, ssexec_t *info)
{
	int r ;
	int n = 0 ;
	char const *nargv[argc + 1] ;
	
	nargv[n++] = "fake_name" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		int f = 0 ;
		for (;;)
		{
			int opt = subgetopt_r(argc,argv, "hv:l:t:T:", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' : 	info_help(info->help); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(info->usage) ; break ;
				case 'l' : 	if (!stralloc_cats(&info->live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&info->live)) retstralloc(111,"main") ;
							break ;
				case 't' : 	if(!stralloc_cats(&info->tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&info->tree)) retstralloc(111,"main") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &info->timeout)) exitusage(info->usage) ; break ;
				default	:	for (int i = 0 ; i < n ; i++)
							{ 
								if (obstr_equal(nargv[i],argv[l.ind]))
									f = 1 ;
							}
							if (!f) nargv[n++] = argv[l.ind] ;  
							f = 0 ;
							break ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	set_ssinfo(info) ;
	
	for (int i = 0 ; i < argc ; i++ , argv++)
			nargv[n++] = *argv ;
	
	nargv[n] = 0 ;
	
	r = (*func)(n,nargv,envp,info) ;
	
	ssexec_free(info) ;
		
	return r ;
}
	

