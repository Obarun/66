/* 
 * tree_cmd_switcher.c
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

//USAGE "tree_switcher [ -v verbosity ] [ -b backup ] [ -s switch ] tree"
// if -b return 0 if point to original source, return 1 if point to backup
// -s , 0 mean original source, 1 mean backup
int tree_switcher(int argc, char const *const *argv)
{
	unsigned int r, change, back, verbosity ;
	uint32_t what = -1 ;
	
	struct stat st ;
	
	char const *tree = NULL ;
	
	uid_t owner = MYUID ;
	
	stralloc base = STRALLOC_ZERO ;
	
	verbosity = 1 ;
	
	change =  back = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "v:s:b", &l) ;
			if (opt == -1) break ;
			if (opt == -2){ strerr_warnw1x("options must be set first") ; return -1 ; }
			switch (opt)
			{
				case 'v' :  if (!uint0_scan(l.arg, &verbosity)) return -1 ;  break ;
				case 's' : 	change = 1 ; if (!uint0_scan(l.arg, &what)) return -1 ; break ;
				case 'b' :	back = 1 ; break ;
				default : 	return -1 ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) return -1 ;
	if (!change && !back) return -1 ;
	
	tree = *argv ;
	size_t treelen = strlen(tree) ;
	
	if (!set_ownersysdir(&base,owner))
	{
		VERBO3 strerr_warnwu1sys("set owner directory") ;
		return -1 ;
	}
	/** $HOME/66/system/tree/servicedirs */
	base.len-- ;
	size_t symlen ;
	char sym[base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1 + SS_TREE_CURRENT_LEN + 1] ;
	memcpy(sym,base.s,base.len) ;
	memcpy(sym + base.len, SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(sym + base.len + SS_SYSTEM_LEN, "/", 1) ;
	memcpy(sym + base.len + SS_SYSTEM_LEN + 1, tree, treelen) ;
	memcpy(sym + base.len + SS_SYSTEM_LEN + 1 + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	memcpy(sym + base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN, "/", 1) ;
	memcpy(sym + base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1, SS_TREE_CURRENT, SS_TREE_CURRENT_LEN) ;
	symlen = base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1 + SS_TREE_CURRENT_LEN ;
	sym[symlen] = 0 ;
	
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
		
		if (!b) return 0 ;
		
		return 1 ;
	}

	if (change)
	{
		char dstori[base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN + 1] ;
		char dstback[base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen + SS_SVDIRS_LEN + 1] ;
		
		memcpy(dstori, base.s, base.len) ;
		memcpy(dstori + base.len, SS_SYSTEM, SS_SYSTEM_LEN) ;
		memcpy(dstori + base.len + SS_SYSTEM_LEN, "/", 1) ;
		memcpy(dstori + base.len + SS_SYSTEM_LEN + 1, tree, treelen) ;
		memcpy(dstori + base.len + SS_SYSTEM_LEN + 1 + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
		dstori[base.len + SS_SYSTEM_LEN + 1 + treelen + SS_SVDIRS_LEN] = 0 ;
	
		memcpy(dstback, base.s, base.len) ;
		memcpy(dstback + base.len, SS_SYSTEM, SS_SYSTEM_LEN) ;
		memcpy(dstback + base.len + SS_SYSTEM_LEN, SS_BACKUP, strlen(SS_BACKUP)) ;
		memcpy(dstback + base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN, "/", 1) ;
		memcpy(dstback + base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, tree, treelen) ;
		memcpy(dstback + base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
		dstback[base.len + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen + SS_SVDIRS_LEN] = 0 ;
			
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
			if (symlink(dstori,sym) < 0)
			{
				VERBO3 strerr_warnwu2sys("symlink: ", dstori) ;
				return -1 ;
			}
		}	
	}
	
	stralloc_free(&base) ;
	
	return 1 ;		
}
	
int tree_cmd_switcher(unsigned int verbosity,char const *cmd, char const *tree)
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
	
	newargv[m++] = "tree_switcher" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&opts); i++)
		newargv[m++] = gaistr(&opts,i) ;
		
	newargv[m++] = tree ;
	newargv[m++] = 0 ;
	
	r = tree_switcher(newopts,newargv) ;

	genalloc_deepfree(stralist,&opts,stra_free) ;
	
	return r ;
}
