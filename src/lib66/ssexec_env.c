/* 
 * ssexec_env.c
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
//#include <stdio.h>
#include <stdlib.h>//getenv

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
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
	stralloc conf = STRALLOC_ZERO ;
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
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'L' : 	if (replace) exitusage(usage_env) ; list = 1 ; break ;
				case 'd' :	src = l.arg ; break ;
				case 'r' :	if (!stralloc_cats(&var,l.arg) ||
							!stralloc_cats(&var,"\n") ||
							!stralloc_0(&var)) retstralloc(111,"main") ; 
							replace = 1 ; break ;
				case 'e' :	if (replace) exitusage(usage_env) ; 
							edit = 1 ;
							break ;
				default : exitusage(usage_env) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc < 1) exitusage(usage_env) ;
	sv = argv[0] ;
	if (!env_resolve_conf(&sasrc,info->owner)) strerr_diefu1sys(111,"get path of the configuration file") ;
	if (!src) src = sasrc.s ;
	if (!file_readputsa(&salist,src,sv)) strerr_diefu3sys(111,"read: ",src,sv) ;
	if (list)
	{
		if (buffer_putsflush(buffer_1, salist.s) < 0)
			strerr_diefu1sys(111, "write to stdout") ;
		goto freed ;
	}
	else if (replace)
	{
		if (!env_merge_conf(src,sv,&salist,&var,replace)) 
			strerr_diefu2x(111,"merge environment file with: ",var.s) ;
	}
	else if (edit)
	{
		size_t svlen = strlen(sv) ;
		char t[sasrc.len + svlen + 1] ;
		editor = getenv("EDITOR") ;
		if (!editor) strerr_diefu1sys(111,"get EDITOR") ;
		memcpy(t,sasrc.s,sasrc.len) ;
		memcpy(t + sasrc.len,sv,svlen) ;
		t[sasrc.len + svlen] = 0 ;
		char const *const newarg[3] = { editor, t, 0 } ;
		xpathexec_run (newarg[0],newarg,envp) ;
	}
	freed:
		stralloc_free(&conf) ;
		stralloc_free(&sasrc) ;
		stralloc_free(&var) ;
		stralloc_free(&salist) ;
	return 0 ;
}
