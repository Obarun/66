/* 
 * backup_cmd_switcher.c
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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/ssexec.h>

//#include <stdio.h>
//USAGE "backup_switcher [ -v verbosity ] [ -t type ] [ -b backup ] [ -s switch ] tree"
// for -b: return 0 if point to original source, return 1 if point to backup
// for -s: -s0 -> origin, -s1 -> backup ;
int backup_switcher(int argc, char const *const *argv,ssexec_t *info)
{
	unsigned int r, change, back, verbosity, type ;
	uint32_t what = -1 ;
		
	struct stat st ;
	
	char const *tree = NULL ;
			
	verbosity = 1 ;
	
	change =  back = type = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "v:t:s:b", &l) ;
			if (opt == -1) break ;
			if (opt == -2){ strerr_warnw1x("options must be set first") ; return -1 ; }
			switch (opt)
			{
				case 'v' :  if (!uint0_scan(l.arg, &verbosity)) return -1 ;  break ;
				case 't' :	if (!uint0_scan(l.arg, &type)) return -1 ; break ;
				case 's' : 	change = 1 ; if (!uint0_scan(l.arg, &what)) return -1 ; break ;
				case 'b' :	back = 1 ; break ;
				default : 	return -1 ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) return -1 ;
	if ((!change && !back) || !type) return -1 ;

	if (type < CLASSIC || type > ONESHOT)
	{
		VERBO3 strerr_warnw1x("unknown type for backup_switcher") ;
		return -1 ;
	}
	tree = *argv ;
	size_t treelen = strlen(tree) ;
	
	
	/** $HOME/66/system/tree/servicedirs */
	//base.len-- ;
	size_t psymlen ;
	char *psym = NULL ;
	if (type == CLASSIC)
	{
		psym = SS_SYM_SVC ;
		psymlen = SS_SYM_SVC_LEN ;
	}
	else
	{
		psym = SS_SYM_DB ;
		psymlen = SS_SYM_DB_LEN ;
	}
	char sym[info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1 + psymlen + 1] ;
	memcpy(sym,info->base.s,info->base.len) ;
	memcpy(sym + info->base.len, SS_SYSTEM,SS_SYSTEM_LEN) ;
	sym[info->base.len + SS_SYSTEM_LEN] = '/' ;
	memcpy(sym + info->base.len + SS_SYSTEM_LEN + 1, tree, treelen) ;
	memcpy(sym + info->base.len + SS_SYSTEM_LEN + 1 + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	sym[info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN] = '/' ;
	memcpy(sym + info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1 ,psym,psymlen) ;
	sym[info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1 + psymlen] = 0 ;
	
	if (back)
	{
		if(lstat(sym,&st) < 0) return -1 ;
		if(!(S_ISLNK(st.st_mode))) 
		{
			VERBO3 strerr_warnwu2x("find symlink: ",sym) ;
			return -1 ;
		}
		stralloc symreal = STRALLOC_ZERO ;
		
		r = sarealpath(&symreal,sym) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu2sys("retrieve real path from: ",sym) ;
			return -1 ;
		}
		char *b = NULL ;
		b = memmem(symreal.s,symreal.len,SS_BACKUP,SS_BACKUP_LEN) ;
		
		stralloc_free(&symreal) ;
		if (!b) return SS_SWSRC ;
		
		return SS_SWBACK ;
	}

	if (change)
	{
		size_t psrclen ;
		size_t pbacklen ;
		char *psrc = NULL ;
		char *pback = NULL ;
		
		if (type == CLASSIC)
		{
			psrc = SS_SVC ;
			psrclen = SS_SVC_LEN ;
			
			pback = SS_SVC ;
			pbacklen = SS_SVC_LEN ;
		}
		else
		{
			psrc = SS_DB ;
			psrclen = SS_DB_LEN ;
			
			pback = SS_DB ;
			pbacklen = SS_DB_LEN ;
		}
	
		char dstsrc[info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + psrclen + 1] ;
		char dstback[info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen + pbacklen + 1] ;
			
		memcpy(dstsrc, info->base.s, info->base.len) ;
		memcpy(dstsrc + info->base.len, SS_SYSTEM, SS_SYSTEM_LEN) ;
		dstsrc[info->base.len + SS_SYSTEM_LEN] = '/' ;
		memcpy(dstsrc + info->base.len + SS_SYSTEM_LEN + 1, tree, treelen) ;
		memcpy(dstsrc + info->base.len + SS_SYSTEM_LEN + 1 + treelen, SS_SVDIRS,SS_SVDIRS_LEN) ;
		memcpy(dstsrc + info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN,psrc,psrclen) ;
		dstsrc[info->base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + psrclen] = 0 ;
					
		memcpy(dstback, info->base.s, info->base.len) ;
		memcpy(dstback + info->base.len, SS_SYSTEM, SS_SYSTEM_LEN) ;
		memcpy(dstback + info->base.len + SS_SYSTEM_LEN, SS_BACKUP, strlen(SS_BACKUP)) ;
		dstback[info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN] = '/' ;
		memcpy(dstback + info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, tree, treelen) ;
		memcpy(dstback + info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen, pback,pbacklen) ;
		dstback[info->base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen + pbacklen] = 0 ;
				
		
		
		if (what >= 0)
		{
			if (unlink(sym) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove symlink: ", sym) ;
				return -1 ;
			}
		}
		
		if (what)
		{
			
			if (symlink(dstback,sym) < 0)
			{
				VERBO3 strerr_warnwu2sys("symlink: ", dstback) ;
				return -1 ;
			}	
		}
		if (!what)
		{
			if (symlink(dstsrc,sym) < 0)
			{
				VERBO3 strerr_warnwu2sys("symlink: ", psrc) ;
				return -1 ;
			}
		}	
	}
	
	return 1 ;		
}
	
int backup_cmd_switcher(unsigned int verbosity,char const *cmd,ssexec_t *info)
{	
	int r ;
	
	genalloc opts = GENALLOC_ZERO ;
	
	if (!clean_val(&opts,cmd))
	{
		VERBO3 strerr_warnwu2x("clean: ",cmd) ;
		return -1 ;
	}
	int newopts = 5 + genalloc_len(stralist,&opts) ;
	char const *newargv[newopts] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, verbosity)] = 0 ;
	
	newargv[m++] = "backup_switcher" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&opts); i++)
		newargv[m++] = gaistr(&opts,i) ;
		
	newargv[m++] = info->treename.s ;
	newargv[m++] = 0 ;
	
	r = backup_switcher(newopts,newargv,info) ;

	genalloc_deepfree(stralist,&opts,stra_free) ;
	
	return r ;
}
