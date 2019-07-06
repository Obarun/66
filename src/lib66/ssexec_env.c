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
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/buffer.h>
#include <skalibs/diuint32.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/parser.h>
#include <66/environ.h>

int ssexec_env(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int list = 0, replace = 0 ;
	stralloc conf = STRALLOC_ZERO ;
	stralloc var = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	
	char const *sv = 0, *src = 0 ;

	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">Ld:r:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'L' : 	if (replace) exitusage(usage_env) ; list = 1 ; break ;
				case 'd' :	src = l.arg ; break ;
				case 'r' :	if (!stralloc_cats(&var,l.arg) ||
							!stralloc_0(&var)) retstralloc(111,"main") ; 
							replace = 1 ; break ;
				default : exitusage(usage_env) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(usage_env) ;
	sv = argv[0] ;
	if (!env_resolve_conf(&sasrc,sv,info->owner)) strerr_diefu1sys(111,"get path of the configuration file") ;
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
		stralloc newlist = STRALLOC_ZERO ;
		// key=value from cmdline
		genalloc mga = GENALLOC_ZERO ;// diuint32
		stralloc msa = STRALLOC_ZERO ;
		// key=value from file
		genalloc ga = GENALLOC_ZERO ;// diuint32
		stralloc sa = STRALLOC_ZERO ;
		char *key, *val, *mkey, *mval ;
		if (!env_clean(&var)) strerr_diefu1x(111,"clean key=value pair") ;
		if (!env_split_one(var.s,&mga,&msa)) strerr_diefu1x(111,"split key=value pair") ;
		mkey = msa.s + genalloc_s(diuint32,&mga)[0].left ;
		mval = msa.s + genalloc_s(diuint32,&mga)[0].right ;
		if (!env_split(&ga,&sa,&salist)) strerr_diefu3x(111,"split key=value pair of file: ",src,sv) ;
		for (unsigned int i = 0 ; i < genalloc_len(diuint32,&ga) ; i++)
		{
			key = sa.s + genalloc_s(diuint32,&ga)[i].left ;
			val = sa.s + genalloc_s(diuint32,&ga)[i].right ;
			if (!stralloc_cats(&newlist,key) ||
			!stralloc_cats(&newlist,"=")) retstralloc(111,"replace") ;
			
			if (obstr_equal(key,mkey)) {
				if (!stralloc_cats(&newlist,mval)) retstralloc(111,"replace") ;
			}else if (!stralloc_cats(&newlist,val)) retstralloc(111,"replace") ;
			if (!stralloc_cats(&newlist,"\n")) retstralloc(111,"replace") ;
		}
		if (!file_write_unsafe(src,sv,newlist.s,newlist.len))
			strerr_diefu3sys(111,"write: ",src,sv) ;
	
		stralloc_free(&newlist) ;
		stralloc_free(&sa) ;
		stralloc_free(&msa) ;
		genalloc_free(diuint32,&mga) ;
		genalloc_free(diuint32,&ga) ;
	}
	freed:
		stralloc_free(&conf) ;
		stralloc_free(&sasrc) ;
		stralloc_free(&var) ;
		stralloc_free(&salist) ;
	return 0 ;
}
