/* 
 * find_sv_src.c
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

#include <66/utils.h>

#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/error2.h>

#include <skalibs/djbunix.h>

#include <66/enum.h>
#include <66/constants.h>


int find_sv_src(stralloc *sa, char const *workdir, char const *name, unsigned int *type)
{
	int r ;
	size_t namelen = strlen(name) ;
	size_t workdirlen = strlen(workdir) ;
	size_t newlen ;
	char src[workdirlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen + 5 + 1] ;
	
	memcpy(src,workdir,workdirlen) ;
	memcpy(src + workdirlen,SS_SVC,SS_SVC_LEN) ;
	newlen = workdirlen + SS_SVC_LEN ;
	memcpy(src + workdirlen + SS_SVC_LEN, "/", 1) ;
	memcpy(src + workdirlen + SS_SVC_LEN + 1, name, namelen) ;
	src[workdirlen + SS_SVC_LEN + 1 + namelen] = 0 ;
	r = scan_mode(src,S_IFDIR) ;
	if (r) 
	{
		if (!stralloc_catb(sa,src,newlen)) retstralloc(0,"find_sv_src") ;
		if (!stralloc_0(sa)) retstralloc(0,"find_sv_src") ;
		*type = CLASSIC ;
		return 1 ;
	}
	memcpy(src,workdir,workdirlen) ;
	memcpy(src + workdirlen,SS_DB,SS_DB_LEN) ;
	memcpy(src + workdirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	newlen = workdirlen + SS_DB_LEN + SS_SRC_LEN ;
	memcpy(src + workdirlen + SS_DB_LEN + SS_SRC_LEN, "/", 1) ;
	memcpy(src + workdirlen + SS_DB_LEN + SS_SRC_LEN + 1, name, namelen) ;
	src[workdirlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen] = 0 ;

	r = scan_mode(src,S_IFDIR) ;
	if (r)
	{
		
		char ctype[8] ;//8->longrun
		if (!stralloc_catb(sa,src,newlen)) retstralloc(0,"find_sv_src") ;
		if (!stralloc_0(sa)) retstralloc(0,"find_sv_src") ;
		newlen = workdirlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen ;
		memcpy(src + newlen, "/type",5) ;
		src[newlen + 5] = 0 ;
		r = openreadnclose(src,ctype,8) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu2sys("read: ",src) ;
			return 0 ;
		}
		*type = get_enumbyid(ctype,key_enum_el) ;
		
		if (type < 0)
		{
			VERBO3 strerr_warnw2x("unknown type: ", ctype) ;
			return 0 ;
		}
		return 1 ;
	}
	
	return 0 ;
}
