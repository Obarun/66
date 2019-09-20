/* 
 * instance.c
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

#include <66/utils.h>

#include <string.h>

#include <oblibs/error2.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/enum.h>

/** New functions */

int instance_check(char const *svname)
{
	int r ;
	r = get_len_until(svname,'@') ;
	// avoid empty value after the instance template name
	if (strlen(svname+r) <= 1 && r > 0) return 0 ;
	
	return r ;
}

int instance_splitname(stralloc *sa,char const *name,int len,int what)
{
	char const *copy ;
	size_t tlen = len + 1 ;
	
	char template[tlen + 1] ;
	memcpy(template,name,tlen) ;
	template[tlen] = 0 ;
	
	copy = name + tlen ;
	
	sa->len = 0 ;
	if (!what)
		if (!stralloc_cats(sa,template) ||
		!stralloc_0(sa)) return 0 ;
	else
		if (!stralloc_catb(sa,copy,strlen(copy)) ||
		!stralloc_0(sa)) return 0 ;
	return 1 ;
}

int instance_create(stralloc *sasv,char const *svname, char const *regex, char const *src, int len)
{
	char const *copy ;
	size_t tlen = len + 1 ;
		
	stralloc tmp = STRALLOC_ZERO ;	
	
	char template[tlen + 1] ;
	memcpy(template,svname,tlen) ;
	template[tlen] = 0 ;
	
	copy = svname + tlen ;

	if (!file_readputsa(&tmp,src,template)) {
		VERBO3 strerr_warnwu3sys("open: ",src,template) ;
		goto err ;
	}
	if (!sastr_replace_all(&tmp,regex,copy)){
		VERBO3 strerr_warnwu3x("replace instance character at: ",src,template) ;
		goto err ;
	}
	if (!stralloc_copy(sasv,&tmp)) goto err ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

/*********************
 * Deprecated function 
 * *******************/

int insta_replace(stralloc *sa,char const *src,char const *cpy)
{
	
	int curr, count ;
	
	if (!src || !*src) return 0;
	
	size_t len = strlen(src) ;
	size_t clen= strlen(cpy) ;
			
	curr = count = 0 ;
	for(int i = 0; (size_t)i < len;i++)
		if (src[i] == '@')
			count++ ;
		
	size_t resultlen = len + (clen * count) ;
	char result[resultlen + 1 ] ;
	
	for(int i = 0; (size_t)i < len;i++)
	{
		if (src[i] == '@')
		{
			
			if (((size_t)i + 1) == len) break ;
			if (src[i + 1] == 'I')
			{
				memcpy(result + curr,cpy,clen) ;
				curr = curr + clen;
				i = i + 2 ;
			}
		}
		result[curr++] = src[i] ;	
			
	}
	result[curr] = 0 ;
	sa->len = 0 ;
	if (!stralloc_cats(sa,result) ||
	!stralloc_0(sa)) return 0 ;
	return 1 ;
}

/** instance -> 0, copy -> 1 */
int insta_splitname(stralloc *sa,char const *name,int len,int what)
{
	char const *copy ;
	size_t tlen = len + 1 ;
	
	char template[tlen + 1] ;
	memcpy(template,name,tlen) ;
	template[tlen] = 0 ;
	
	copy = name + tlen ;
	
	sa->len = 0 ;
	if (!what)
		if (!stralloc_cats(sa,template) ||
		!stralloc_0(sa)) return 0 ;
	else
		if (!stralloc_catb(sa,copy,strlen(copy)) ||
		!stralloc_0(sa)) return 0 ;
	return 1 ;
}

int insta_create(stralloc *sasv,stralloc *sv, char const *src, int len)
{
	char *fdtmp ;
	char const *copy ;
	size_t tlen = len + 1 ;
	
	stralloc sa = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;	
	
	char template[tlen + 1] ;
	memcpy(template,sv->s,tlen) ;
	template[tlen] = 0 ;
	
	copy = sv->s + tlen ;

	fdtmp = dir_create_tmp(&tmp,"/tmp",copy) ;
	if(!fdtmp)
	{
		VERBO3 strerr_warnwu1x("create instance tmp dir") ;
		return 0 ;
	}

	if (!file_readputsa(&sa,src,template)){
		VERBO3 strerr_warnwu4sys("open: ",src,"/",template) ;
		return 0 ;
	}
	
	if (!insta_replace(&sa,sa.s,copy)){
		VERBO3 strerr_warnwu2x("replace instance character at: ",sa.s) ;
		return 0 ;
	}
	/** remove the last \0 */
	sa.len-- ;
	
	if (!file_write_unsafe(tmp.s,copy,sa.s,sa.len)){
		VERBO3 strerr_warnwu4sys("create instance service file: ",tmp.s,"/",copy) ;
		return 0 ;
	}
	
	if (!read_svfile(sasv,copy,tmp.s)) return 0 ;
	
	if (rm_rf(tmp.s) < 0){
		VERBO3 strerr_warnwu2x("remove tmp directory: ",tmp.s) ;
		return 0 ;
	}
	
	stralloc_free(&sa) ;
	stralloc_free(&tmp) ;
	sv->len = 0 ;
	if (!stralloc_catb(sv,copy,strlen(copy)) ||
	!stralloc_0(sv)) return 0 ;
	return 1 ;
}

int insta_check(char const *svname)
{
	int r ;
		
	r = get_len_until(svname,'@') ;
	if (r < 0) return -1 ;
	
	return r ;
}
