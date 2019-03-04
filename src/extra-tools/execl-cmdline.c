/* 
 * execl-cmdline.c
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

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/error2.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/sgetopt.h>

#include <execline/execline.h>

#define USAGE "execl-cmdline [ -s ] { command... }"

int main(int argc, char const **argv, char const *const *envp)
{

	int r, argc1, split ;

	PROG = "execl-cmdline" ;
	
	stralloc tmodifs = STRALLOC_ZERO ;
	stralloc modifs = STRALLOC_ZERO ;
	genalloc ga = GENALLOC_ZERO ;
	
	r =  argc1 = split = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
		  int opt = subgetopt_r(argc, argv, "s", &l) ;
		  if (opt == -1) break ;
		  switch (opt)
		  {
			case 's' : split = 1 ; break ;
			default : exitusage(USAGE) ;
		  }
		}
		argc -= l.ind ; argv += l.ind ;
	}
	argc1 = el_semicolon(argv) ;
	if (argc1 >= argc) strerr_dief1x(100, "unterminated block") ;
	argv[argc1] = 0 ;
	
	char const **newargv = argv ;
	for (int i = 0; i < argc1 ; i++, newargv++)
	{	
		if (!*newargv[0])
			continue ;
		
		if (!stralloc_cats(&tmodifs,*newargv)) retstralloc(111,"tmodifs") ;
		if (split)
		{
			if (!stralloc_cats(&tmodifs," ")) retstralloc(111,"tmodifs") ;
		}
		else if (!stralloc_0(&tmodifs)) retstralloc(111,"tmodifs_0") ;
		r++;
	}
	
	if (split)
	{
		if (!clean_val(&ga,tmodifs.s)) strerr_diefu2x(111,"clean val: ",tmodifs.s) ;
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&ga) ; i++)
		{
			stralloc_cats(&modifs,gaistr(&ga,i)) ;
			stralloc_0(&modifs) ;
		}
		r = genalloc_len(stralist,&ga) ;
		genalloc_deepfree(stralist,&ga,stra_free) ;
	}
	else if (!stralloc_copy(&modifs,&tmodifs)) retstralloc(111,"copy") ;
	
	stralloc_free(&tmodifs) ;
		
	char const *newarg[r + 1] ;
    if (!env_make(newarg, r, modifs.s, modifs.len)) strerr_diefu1sys(111, "env_make") ;
    newarg[r] = 0 ;
		
	xpathexec_run(newarg[0],newarg,envp) ;
}
