/* 
 * 66-execl.c
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

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/error2.h>

#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>


int main(int argc, char const *const *argv, char const *const *envp)
{

	int r ;
	PROG = "66-execl" ;
	genalloc ga = GENALLOC_ZERO ; 
	if (argc < 2) strerr_diefu1x(111,"missing arguments") ;
	argc-- ; argv++ ;	
	for (int i = 0 ;i < argc;i++,argv++)
	{
		r = clean_val(&ga,*argv) ;
		if (!r) strerr_diefu2x(111,"clean_val: ",*argv) ;
	}
	
	unsigned int m = genalloc_len(stralist,&ga) ;
	char const *newarg[m+1] ;
	unsigned int n = 0 ;
	for (unsigned int i =0; i < genalloc_len(stralist,&ga); i++)
		newarg[n++] = gaistr(&ga,i) ;
	
	newarg[n] = 0 ;
			
	xpathexec_run(newarg[0],newarg,envp) ;
}
