/* 
 * resolve.c
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

#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>//realpath

#include <oblibs/types.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>

/** @Return -2 on error
 * @Return -1 if resolve directory doesn't exist
 * @Return 0 is file doesn't exist
 * @Return 1 on success */
int resolve_read(stralloc *sa, char const *src, char const *svname,char const *file)
{
	int r ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(svname) ;
	size_t filelen = strlen(file) ;
	size_t newlen ;
	
	char solve[srclen + SS_RESOLVE_LEN + 1 + namelen + 7 + 1] ;
	memcpy(solve,src,srclen) ;
	memcpy(solve + srclen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	solve[srclen + SS_RESOLVE_LEN] = '/' ;
	memcpy(solve + srclen + SS_RESOLVE_LEN + 1, svname,namelen) ;
	solve[srclen +  SS_RESOLVE_LEN + 1 + namelen] = '/' ;
	newlen = srclen +  SS_RESOLVE_LEN + 1 + namelen + 1 ;
	solve[newlen] = 0 ;
	
	r = scan_mode(solve,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2x("invalid directory: ",solve) ;
		return -2 ;
	}
	if (!r) return -1 ;
	
	memcpy(solve + newlen,file,filelen) ;
	solve[newlen + filelen] = 0 ;
	
	*sa = stralloc_zero ;
	r = openslurpclose(sa,solve) ;
	if (!r) return 0 ;
	//if (!sa->len) return 0 ;
	if (!stralloc_0(sa)) return -1 ;

	return 1 ;
} 

int resolve_write(char const *dst,char const *svname, char const *file, char const *contents,unsigned int force)
{
	int r ;
	
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(svname) ;
	size_t clen = strlen(contents) ;
	size_t filen = strlen(file) ;
	
	size_t newlen ;
	char solve[dstlen + SS_RESOLVE_LEN + 1 + namelen + 2] ;
	
	memcpy(solve, dst, dstlen) ;
	memcpy(solve + dstlen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	solve[dstlen + SS_RESOLVE_LEN] = '/' ;
	newlen = dstlen + SS_RESOLVE_LEN + 1 ;
	memcpy(solve + newlen,svname,namelen) ;
	solve[newlen + namelen] = 0 ;
	
	r = scan_mode(solve,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2sys("invalid directory: ",solve) ; 
		return 0 ;
	}
	if (!r)
	{
		solve[newlen] = 0 ;
		if (!dir_create_under(solve,svname,0755))
		{
			VERBO3 strerr_warnwu3sys("create directory: ",solve,svname) ;
			return 0 ;
		}
	}
			
	memcpy(solve + newlen,svname,namelen) ;
	solve[newlen + namelen] = '/' ;
	solve[newlen + namelen + 1] = 0 ;
	newlen = newlen + namelen + 1 ;

	char torm[newlen + filen + 1] ;
	memcpy(torm,solve,newlen) ;
	memcpy(torm + newlen,file,filen) ;
	torm[newlen + filen] = 0 ;
	r = scan_mode(torm,S_IFREG) ;
	if (r < 0 || (r && force))
	{
		if (rm_rf(torm) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",torm) ;
			return 0 ;
		}
	}

	if (!file_write_unsafe(solve,file,contents,clen))
	{
		VERBO3 strerr_warnwu3sys("write revolve file: ",solve,file) ;
		return 0 ;
	}
	
	return 1 ;
}

int resolve_remove(char const *dst,char const *svname,char const *file)
{
	int r ;
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(svname) ;
	size_t filen = strlen(file) ;

	size_t newlen ;
	char solve[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1 + filen + 1] ;
	
	memcpy(solve, dst, dstlen) ;
	memcpy(solve + dstlen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	solve[dstlen + SS_RESOLVE_LEN] = '/' ;
	memcpy(solve + dstlen + SS_RESOLVE_LEN + 1,svname,namelen) ;
	newlen = dstlen + SS_RESOLVE_LEN + 1 + namelen ;
	solve[newlen] = 0 ;

	r = scan_mode(solve,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2sys("invalid directory: ",solve) ; 
		return 0 ;
	}
	if (!r)
	{
		VERBO3 strerr_warnwu2sys("find directory: ",solve) ;
		return 0 ;
	
	}
	solve[newlen] = '/' ;
	memcpy(solve + newlen + 1,file,filen) ;
	solve[newlen + 1 + filen] = 0 ;

	r = scan_mode(solve,S_IFREG) ;
	if (r < 0 || r)
	{
		if (rm_rf(solve) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",solve) ;
			return 0 ;
		}
	}
	
	return 1 ;
}
int resolve_remove_service(char const *dst, char const *svname)
{
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(svname) ;
	
	char solve[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	
	memcpy(solve, dst, dstlen) ;
	memcpy(solve + dstlen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	solve[dstlen + SS_RESOLVE_LEN] = '/' ;
	memcpy(solve + dstlen + SS_RESOLVE_LEN + 1,svname,namelen) ;
	solve[dstlen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;

	if (scan_mode(solve,S_IFDIR))
	{
		if (rm_rf(solve) < 0)
		{
			VERBO3 strerr_warnw2sys("remove directory: ",solve) ; 
			return 0 ;
		}
	}

	return 1 ;
}
int resolve_symlive(char const *live, char const *tree, char const *treename)
{
	int r ;
	
	size_t livelen = strlen(live) - 1 ;
	size_t treelen = strlen(tree) ;
	size_t treenamelen = strlen(treename) ;
	size_t newlen ;
	
	uid_t owner = MYUID ;
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,owner) ;
	ownerstr[ownerlen] = 0 ;
	struct stat st ;
	
	char dst[treelen + SS_SVDIRS_LEN + 1] ;
	memcpy(dst,tree,treelen) ;
	memcpy(dst + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	dst[treelen + SS_SVDIRS_LEN] = 0 ;
	
	char sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen + SS_RESOLVE_LEN + 1] ;
	memcpy(sym, live,livelen) ;
	memcpy(sym + livelen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	sym[livelen + SS_RESOLVE_LEN] = '/' ;
	memcpy(sym + livelen + SS_RESOLVE_LEN + 1, ownerstr,ownerlen) ;
	newlen = livelen + SS_RESOLVE_LEN + 1 + ownerlen ;
	sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen] = '/' ;
	memcpy(sym + livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1, treename,treenamelen) ;
	sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen] = 0 ;
	
	r = scan_mode(sym,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2x("invalid directory: ",sym) ;
		return 0 ;
	}
	if (!r)
	{
		sym[newlen] = 0 ;
		if (!dir_create_under(sym,treename,0755))
		{
			VERBO3 strerr_warnwu4sys("create directory: ",sym,"/",treename) ;
			return 0 ;
		}
		sym[newlen] = '/' ;
		memcpy(sym + newlen + 1, treename,treenamelen) ;
		sym[livelen + newlen + 1 + treenamelen] = 0 ;
	}
	memcpy(sym + newlen + 1 + treenamelen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	sym[newlen + 1 + treenamelen + SS_RESOLVE_LEN] = 0 ;
		
	if(lstat(sym,&st) < 0)
	{
		if (symlink(dst,sym) < 0)
		{
			VERBO3 strerr_warnwu4x("point symlink: ",sym," to ",dst) ;
			return 0 ;
		}
	}
	if (unlink(sym) < 0)
	{
		VERBO3 strerr_warnwu2sys("unlink: ",sym) ;
		return 0 ;
	}
	if (symlink(dst,sym) < 0)
	{
		VERBO3 strerr_warnwu4x("point symlink: ",sym," to ",dst) ;
		return 0 ;
	}
		
	return 1 ;
}

int resolve_pointo(stralloc *sa,char const *base, char const *live,char const *tree,char const *treename,unsigned int type, unsigned int what)
{
	size_t baselen = strlen(base)  ;
	size_t treelen = strlen(tree) ;
	size_t livelen = strlen(live) - 1 ;
	size_t treenamelen = strlen(treename) ;
	size_t sourcelen ;
	size_t backlen ;
	size_t psrclen ;
	
	char *r = NULL ;
	uid_t owner = MYUID ;
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,owner) ;
	ownerstr[ownerlen] = 0 ;

	char resolve[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen + 1] ;
	memcpy(resolve, live,livelen) ;
	memcpy(resolve + livelen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	resolve[livelen + SS_RESOLVE_LEN] = '/' ;
	memcpy(resolve + livelen + SS_RESOLVE_LEN + 1, ownerstr,ownerlen) ;
	resolve[livelen + SS_RESOLVE_LEN + 1 + ownerlen] = '/' ;
	memcpy(resolve + livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1, treename,treenamelen) ;
	resolve[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen] = 0 ;
	
	char source[treelen + SS_SVDIRS_LEN + SS_SVC_LEN + 1] ;
	memcpy(source,tree,treelen) ;
	memcpy(source + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	sourcelen = treelen + SS_SVDIRS_LEN ;
	source[sourcelen] = 0 ;
	
	char backup[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen + SS_SVC_LEN + 1] ;
	memcpy(backup,base,baselen) ;
	memcpy(backup + baselen, SS_SYSTEM, SS_SYSTEM_LEN) ;
	memcpy(backup + baselen + SS_SYSTEM_LEN, SS_BACKUP, SS_BACKUP_LEN) ;
	backup[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN] = '/' ;
	memcpy(backup + baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, treename,treenamelen) ;
	backlen = baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen ;
	backup[backlen] = 0 ;
	
	if (type && what)
	{ 
		char psrc[SS_DB_LEN + SS_SRC_LEN + 1] ;

		if (type == CLASSIC)
		{
			memcpy(psrc,SS_SVC, SS_SVC_LEN) ;
			psrclen = SS_SVC_LEN ;
		}
		else
		{
			memcpy(psrc,SS_DB, SS_DB_LEN) ;
			//memcpy(psrc + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
			//psrclen = SS_DB_LEN + SS_SRC_LEN ;
			psrclen = SS_DB_LEN ;
		}
		memcpy(source + sourcelen,psrc, psrclen) ;
		source[sourcelen + psrclen] = 0 ;
		memcpy(backup + backlen, psrc, psrclen) ;
		backup[backlen + psrclen] = 0 ;
	}
	
	if (!what) r = resolve ;
	if (what == 1) r = source ;
	if (what > 1) r = backup ;

	return stralloc_obreplace(sa,r) ;
}
