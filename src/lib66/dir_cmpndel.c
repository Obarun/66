/* 
 * dir_cmpndel.c
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

#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <66/utils.h>

int dir_cmpndel(char const *src, char const *dst,char const *exclude)
{
	
	int fdsrc,r ;
	
	size_t dstlen = strlen(dst) ;
	
	DIR *dir = opendir(dst) ;
	
	if (!dir)
		strerr_diefu2sys(111,"open : ", dst) ;
	
	fdsrc = dir_fd(dir) ;
	
	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		if (stat_at(fdsrc, d->d_name, &st) < 0)
		{
			VERBO3 strerr_warnwu4sys("stat ", dst, "/", d->d_name) ;
			return 0 ;
		}
		if (obstr_equal(d->d_name,exclude)) continue ;
		
		r = dir_search(src,d->d_name,S_IFDIR) ;
		if(r)
		{
			char del[dstlen + 1 + strlen(d->d_name) + 1] ;
			memcpy(del,dst,dstlen) ;
			memcpy(del + dstlen, "/",1) ;
			memcpy(del + dstlen + 1, d->d_name,strlen(d->d_name)) ;
			del[dstlen + 1 + strlen(d->d_name)] = 0 ;
			if (rm_rf(del) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove: ",del) ;
				return 0 ;
			}
		}
	}
	return 1 ;
}
