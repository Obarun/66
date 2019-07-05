/* 
 * execl-subuidgid.c
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
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <oblibs/error2.h>

#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>

#include <execline/execline.h>
#include <66/environ.h>
#include <66/utils.h>

#define USAGE "execl-subuidgid [ -h ] [ -o owner ] prog..."

static inline void info_help (void)
{
  static char const *help =
"execl-subuidgid <options> prog\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-o: owner to use\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int main (int argc, char const **argv, char const *const *envp)
{
	uid_t uid ;
	gid_t gid ;
	int r ;
	char const *owner = 0 ;
	stralloc sa = STRALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	exlsn_t info = EXLSN_ZERO;
	char cuid[UID_FMT], cgid[GID_FMT] ;
	
	PROG = "execl-subuidgid" ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
		  int opt = subgetopt_r(argc, argv, "ho:", &l) ;
		  if (opt == -1) break ;
		  switch (opt)
		  {
			case 'h' : info_help(); return 0 ;
			case 'o' : owner = l.arg ; break ;
			default : exitusage(USAGE) ;
		  }
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (owner)
	{
		if (!youruid(&uid,owner)) strerr_diefu2sys(111,"get uid of: ",owner) ;
	}
	else uid = getuid() ;
		
	if (!yourgid(&gid,uid)) strerr_diefu1sys(111,"get gid") ;
	cuid[uid_fmt(cuid,uid)] = 0 ;
	cgid[gid_fmt(cgid,gid)] = 0 ;
	
	if (!env_addkv("UID",cuid,&info)) strerr_diefu1sys(111,"set UID") ;
	if (!env_addkv("GID",cgid,&info)) strerr_diefu1sys(111,"set GID") ;
	
	if (!env_string(&sa,argv,(unsigned int) argc)) strerr_diefu1sys(111,"environment string") ;
	
	r = el_substitute (&dst, sa.s, sa.len, info.vars.s, info.values.s,
		genalloc_s (elsubst_t const, &info.data),genalloc_len (elsubst_t const, &info.data)) ;
	if (r < 0) strerr_diefu1sys(111,"el_substitute") ;
	else if (!r) _exit(0) ;
	
	stralloc_free(&sa) ;
	
	{
        char const *v[r + 1];
        if (!env_make (v, r, dst.s, dst.len)) strerr_diefu1sys (111, "env_make") ;
        v[r] = 0 ;
        pathexec_r (v, envp, env_len (envp), info.modifs.s, info.modifs.len) ;
    }

	return 0 ;	
}
