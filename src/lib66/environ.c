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
	stralloc skey = STRALLOC_ZERO ;
	stralloc sval = STRALLOC_ZERO ;
	stralloc mkey = STRALLOC_ZERO ;
	stralloc mval = STRALLOC_ZERO ;
	if (!environ_get_clean_env(srclist) ||
	!environ_get_clean_env(modifs) ||
	!environ_clean_nline(srclist) ||
	!environ_clean_nline(modifs) ||
	!stralloc_copy(&result,srclist)) goto err ;
	result.len-- ; //remove 0
	if (!sastr_split_string_in_nline(modifs)) goto err ;
	if (!sastr_split_string_in_nline(srclist)) goto err ;
	for (;pos < modifs->len; pos += strlen(modifs->s + pos) + 1)
	{
		fakepos = pos ;
		skey.len = sval.len = mkey.len = mval.len = 0 ;
		if (!stralloc_copy(&mkey,modifs) ||
		!stralloc_copy(&mval,modifs) ||
		!stralloc_copy(&skey,srclist) ||
		!stralloc_copy(&sval,srclist)) goto err ;
		if(!environ_get_key_nclean(&mkey,&pos)) goto err ;
		r = sastr_find(srclist,mkey.s) ;
		if (r >= 0)
		{
			if (force) 
			{
				if (!environ_get_val_of_key(&sval,mkey.s) ||
				!environ_get_val_of_key(&mval,mkey.s) ||
				!sastr_replace(&result,sval.s,mval.s)) goto err ;
				result.len-- ;
			}
		}
		else
		{
			if (!stralloc_cats(&result,modifs->s+fakepos) ||
			!stralloc_cats(&result,"\n")) goto err ;	
		}
	}
	if (!stralloc_cats(&result,"\n")) goto err ;
	if (!file_write_unsafe(dst,file,result.s,result.len)) goto err ;
	
	stralloc_free(&result) ; stralloc_free(&skey) ;
	stralloc_free(&sval) ; stralloc_free(&mkey) ;
	stralloc_free(&mval) ;
	return 1 ;
	err:
		stralloc_free(&result) ; stralloc_free(&skey) ;
		stralloc_free(&sval) ; stralloc_free(&mkey) ;
		stralloc_free(&mval) ;
		return 0 ;
}
