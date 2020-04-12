/* 
 * ssexec_env.c
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
//#include <stdio.h>
#include <stdlib.h>//getenv

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/buffer.h>
#include <skalibs/diuint32.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/parser.h>
#include <66/environ.h>

int ssexec_env(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int list = 0, replace = 0 , edit = 0 ;
	stralloc result = STRALLOC_ZERO ;
	stralloc var = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	
	char const *sv = 0, *src = 0, *editor ;

	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">Ld:r:e", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'L' : 	if (replace) log_usage(usage_env) ; list = 1 ; break ;
				case 'd' :	src = l.arg ; break ;
				case 'r' :	if (!stralloc_cats(&var,l.arg) ||
							!stralloc_cats(&var,"\n") ||
							!stralloc_0(&var)) log_die_nomem("stralloc") ; 
							replace = 2 ; break ;
				case 'e' :	if (replace) log_usage(usage_env) ; 
							edit = 1 ;
							break ;
				default : 	log_usage(usage_env) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc < 1) log_usage(usage_env) ;
	sv = argv[0] ;
	if (!env_resolve_conf(&sasrc,info->owner)) log_dieusys(LOG_EXIT_SYS,"get path of the configuration file") ;
	if (!src) src = sasrc.s ;
	if (!file_readputsa(&salist,src,sv))log_dieusys(LOG_EXIT_SYS,"read: ",src,sv) ;
	if (list)
	{
		if (buffer_putsflush(buffer_1, salist.s) < 0)
			log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
		goto freed ;
	}
	else if (replace)
	{
		if (!env_merge_conf(&result,&salist,&var,replace)) 
			log_dieu(LOG_EXIT_SYS,"merge environment file with: ",var.s) ;
	}
	else if (edit)
	{
		size_t svlen = strlen(sv) ;
		char t[sasrc.len + svlen + 1] ;
		editor = getenv("EDITOR") ;
		if (!editor) log_dieusys(LOG_EXIT_SYS,"get EDITOR") ;
		memcpy(t,sasrc.s,sasrc.len) ;
		memcpy(t + sasrc.len,sv,svlen) ;
		t[sasrc.len + svlen] = 0 ;
		char const *const newarg[3] = { editor, t, 0 } ;
		xpathexec_run (newarg[0],newarg,envp) ;
	}
	freed:
		stralloc_free(&result) ;
		stralloc_free(&sasrc) ;
		stralloc_free(&var) ;
		stralloc_free(&salist) ;
	return 0 ;
}
