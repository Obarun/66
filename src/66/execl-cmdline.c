/* 
 * execl-cmdline.c
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

#include <skalibs/stralloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>

#include <execline/execline.h>

int main(int argc, char const **argv, char const *const *envp)
{

	int r, argc1 ;

	PROG = "66-execl" ;

	stralloc modifs = STRALLOC_ZERO ;
	
	r =  0 ;
	
	argc1 = el_semicolon(++argv) ;
	if (argc1 >= --argc) strerr_dief1x(100, "unterminated block") ;
	
	char const **newargv = argv ;
	for (int i = 0;i<argc1;i++, newargv++)
	{	
		if (!*newargv[0])
			continue ;
		
		stralloc_cats(&modifs,*newargv) ;
		stralloc_0(&modifs) ;
		r++;
	}
	
	char const *newarg[r + 1] ;
    if (!env_make(newarg, r, modifs.s, modifs.len)) strerr_diefu1sys(111, "env_make") ;
    newarg[r] = 0 ;
	
	xpathexec_run(newarg[0],newarg,envp) ;
	//el_execsequence(newarg, argv+argc1+1, envp) ;
}
