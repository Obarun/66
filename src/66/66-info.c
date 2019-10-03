/* 
 * 66-info.c
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

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>

#include <skalibs/buffer.h>

#define USAGE "66-info is deprecated -- use 66-intree for tree informations or 66-inservice for service informations"

static inline void info_help (void)
{
  static char const *help =
"66-info is deprecated -- use 66-intree for tree informations or 66-inservice for service informations\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	int what ;
	
	what = -1 ;
	
	PROG = "66-info" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(2,argv, ">hTS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'T' : 	what = 0 ; break ;
				case 'S' :	what = 1 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
	}
	if (what == -1) exitusage(USAGE) ;
	
	if (!what) {
		strerr_warnw1x("66-info is deprecated -- use 66-intree instead") ;
	}
	else if (what) {
		strerr_warnw1x("66-info is deprecated -- use 66-inservice instead") ;
	}
		
	return 0 ;
}
