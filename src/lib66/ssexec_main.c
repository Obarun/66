/* 
 * ssexec_main.c
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

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/tree.h>

#include <stdio.h>

void set_ssinfo(ssexec_t *info)
{
	int r ;
	
	info->owner = MYUID ;
	
	if (!set_ownersysdir(&info->base,info->owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
	
	r = tree_sethome(&info->tree,info->base.s,info->owner) ;
	if (r < 0) log_dieu(LOG_EXIT_USER,"find the current tree. You must use -t options") ;
	if (!r) log_dieusys(LOG_EXIT_SYS,"find tree: ", info->tree.s) ;
	
	r = tree_setname(&info->treename,info->tree.s) ;
	if (r < 0) log_dieu(LOG_EXIT_SYS,"set the tree name") ;
	
	if (!tree_get_permissions(info->tree.s,info->owner))
		log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;
	else info->treeallow = 1 ;

	r = set_livedir(&info->live) ;
	if (!r) log_die_nomem("stralloc") ;
	if(r < 0) log_die(LOG_EXIT_SYS,"live: ",info->live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&info->livetree,&info->live)) log_die_nomem("stralloc") ;
	if (!stralloc_copy(&info->scandir,&info->live)) log_die_nomem("stralloc") ;
	
	r = set_livetree(&info->livetree,info->owner) ;
	if (!r) log_die_nomem("stralloc") ;
	if(r < 0) log_die(LOG_EXIT_SYS,"livetree: ",info->livetree.s," must be an absolute path") ;
	
	r = set_livescan(&info->scandir,info->owner) ;
	if (!r) log_die_nomem("stralloc") ;
	if(r < 0) log_die(LOG_EXIT_SYS,"scandir: ",info->scandir.s," must be an absolute path") ;
}

static inline void info_help (char const *help)
{
	if (buffer_putsflush(buffer_1, help) < 0)
		log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}



int ssexec_main(int argc, char const *const *argv,char const *const *envp,ssexec_func_t *func, ssexec_t *info)
{
	int r ;
	int n = 0 ;
	char const *nargv[argc + 1] ;
	
	log_color = &log_color_disable ;
	
	nargv[n++] = "fake_name" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		int f = 0 ;
		for (;;)
		{
			int opt = subgetopt_r(argc,argv, "hv:l:t:T:z", &l) ;
			
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' :	info_help(info->help); return 0 ;
				case 'v' :	if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(info->usage) ;
							info->opt_verbo = 1 ;
							break ;
				case 'l' :	if (!stralloc_cats(&info->live,l.arg)) log_die_nomem("stralloc") ;
							if (!stralloc_0(&info->live)) log_die_nomem("stralloc") ;
							info->opt_live = 1 ;
							break ;
				case 't' :	if(!stralloc_cats(&info->tree,l.arg)) log_die_nomem("stralloc") ;
							if(!stralloc_0(&info->tree)) log_die_nomem("stralloc") ;
							info->opt_tree = 1 ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &info->timeout)) log_usage(info->usage) ;
							info->opt_timeout = 1 ;
							break ;
				case 'z' :	log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
							info->opt_color = 1 ;
							break ;
				default	:	for (int i = 0 ; i < n ; i++)
							{						
								if (!argv[l.ind]) log_usage(info->usage) ;
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
	

