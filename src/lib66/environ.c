/* 
 * environ.c
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

//#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

int env_resolve_conf(stralloc *env, uid_t owner)
{
	if (!owner)
	{
		if (!stralloc_cats(env,SS_SERVICE_ADMCONFDIR)) return 0 ;
	}
	else
	{
		if (!set_ownerhome(env,owner)) return 0 ;
		if (!stralloc_cats(env,SS_SERVICE_USERCONFDIR)) return 0 ;
	}	
	if (!stralloc_0(env)) return 0 ;
	env->len-- ;
	return 1 ;
}

int env_merge_conf(char const *dst,char const *file,stralloc *srclist,stralloc *modifs,unsigned int force)
{
	int r ;
	size_t pos = 0, fakepos = 0 ;
	stralloc result = STRALLOC_ZERO ;
	stralloc sval = STRALLOC_ZERO ;
	stralloc mkey = STRALLOC_ZERO ;
	stralloc mval = STRALLOC_ZERO ;
	
	if (!environ_get_clean_env(srclist) ||
	!environ_clean_nline(srclist) ||
	!stralloc_0(srclist)) goto err ;
	srclist->len-- ;
	
		
	if (!environ_get_clean_env(modifs) ||
	!environ_clean_nline(modifs) ||
	!stralloc_0(modifs)) goto err ;
	modifs->len-- ;
	
	if (!stralloc_copy(&result,srclist) ||
	!sastr_split_string_in_nline(modifs) ||
	!sastr_split_string_in_nline(srclist)) goto err ;
	
	if (!stralloc_0(&result)) goto err ;
	result.len-- ;
			
	for (;pos < modifs->len; pos += strlen(modifs->s + pos) + 1)
	{
		
		fakepos = pos ;
		sval.len = mkey.len = mval.len = 0 ;
		
		if (!stralloc_copy(&mkey,modifs) ||
		!stralloc_copy(&mval,modifs) ||
		!stralloc_copy(&sval,srclist)) goto err ;
		
		if (!environ_get_key_nclean(&mkey,&pos)) goto err ;
		mkey.len-- ;
		
		if (!auto_stra(&mkey,"=")) goto err ;
		r = sastr_find(srclist,mkey.s) ;
		if (r >= 0)
		{
			if (force) 
			{
				if (!environ_get_val_of_key(&sval,mkey.s)) goto err ;// ||
				if (!environ_get_val_of_key(&mval,mkey.s)) goto err ;
				//remove 0
				sval.len-- ;
				mval.len-- ;
				r = sastr_find(&result,mkey.s) ;
				char tmp[result.len + 1] ;
				auto_strings(tmp,result.s) ;
				size_t mlen = r + mkey.len + (mval.len < sval.len ? mval.len : sval.len) ;
				size_t add = sval.len > mval.len ? (sval.len - mval.len) : 0 ;
				result.len = r + mkey.len ;
				stralloc_0(&result) ;
				result.len-- ;
				if (!auto_stra(&result,mval.s,tmp + mlen + add)) goto err ;
			}
			continue ;
		}
		else
		{
			if (!auto_stra(&result,"\n",modifs->s+fakepos,strlen(modifs->s + fakepos))) goto err ;	
		}
		
	}
	if (!auto_stra(&result,"\n")) goto err ;
	
	if (!file_write_unsafe(dst,file,result.s,result.len)) goto err ;
	
	stralloc_free(&result) ;
	stralloc_free(&sval) ; 
	stralloc_free(&mkey) ;
	stralloc_free(&mval) ;
	return 1 ;
	err:
		stralloc_free(&result) ;
		stralloc_free(&sval) ; 
		stralloc_free(&mkey) ;
		stralloc_free(&mval) ;
		return 0 ;
}
