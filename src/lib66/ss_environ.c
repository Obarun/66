/* 
 * ss_environ.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <string.h>

#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/environ.h>

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

int env_merge_conf(stralloc *result,stralloc *srclist,stralloc *modifs,uint8_t conf)
{
	int r, comment ;
	size_t pos = 0 ;
	char *end = 0 ;
	stralloc user = STRALLOC_ZERO ;
	stralloc key = STRALLOC_ZERO ;
	stralloc val = STRALLOC_ZERO ;
	
	if (!auto_stra(&user,"\n## diff from upstream ##\n")) goto err ;

	/** User can empty the file and the function
	 * fail. In that case we rewrite the file entirely */
	if (!env_clean_with_comment(srclist)) {
		if (!auto_stra(result,modifs->s)) goto err ;
		goto freed ;
	}
	 
	if (!env_clean_with_comment(modifs)) goto err ;

	if (!sastr_split_string_in_nline(modifs) ||
	!sastr_split_string_in_nline(srclist)) goto err ;

	for (;pos < modifs->len; pos += strlen(modifs->s + pos) + 1)
	{
		comment = modifs->s[pos] == '#' ? 1 : 0 ;
		key.len = 0 ;
		char *line = modifs->s + pos ;

		/** keep a empty line between key=value pair and a comment */
		end = get_len_until(line,'=') < 0 ? "\n" : "\n\n" ;

		if (comment)
		{
			if (!auto_stra(result,line,end)) goto err ;
			continue ;
		}
		if (!stralloc_copy(&key,modifs)) goto err ;
		if (!environ_get_key_nclean(&key,&pos)) goto err ;
		key.len-- ;
		if (!auto_stra(&key,"=")) goto err ;
		r = sastr_find(srclist,key.s) ;
		if (r >= 0)
		{
			/** apply change from upstream */
			if (conf > 1)
			{
				if (!auto_stra(result,"\n",line,"\n\n")) goto err ;
				continue ;
			}
			/** keep user change */
			else
			{
				if (!stralloc_copy(&val,srclist)) goto err ;
				if (!environ_get_val_of_key(&val,key.s)) goto err ;
				if (!auto_stra(result,"\n",key.s,val.s,"\n\n")) goto err ;
				continue ;
			}
		}

		if (!auto_stra(result,"\n",line,end)) goto err ;	
	}
	/** search for a key added by user */
	for (pos = 0 ; pos < srclist->len ; pos += strlen(srclist->s + pos) + 1)
	{
		comment = srclist->s[pos] == '#' ? 1 : 0 ;
		key.len = 0 ;
		char *line = srclist->s + pos ;

		if (comment) continue ;

		if (!stralloc_copy(&key,srclist)) goto err ;
		if (!environ_get_key_nclean(&key,&pos)) goto err ;
		key.len-- ;
		if (!auto_stra(&key,"=")) goto err ;
		r = sastr_find(modifs,key.s) ;
		if (r >= 0) continue ;

		if (!auto_stra(&user,line,"\n")) goto err ;
	}

	if (user.len > 26)
		if (!auto_stra(result,user.s)) goto err ;

	freed:

	stralloc_free(&user) ; 
	stralloc_free(&key) ;
	stralloc_free(&val) ;
	return 1 ;
	err:
		stralloc_free(&user) ; 
		stralloc_free(&key) ;
		stralloc_free(&val) ;
		return 0 ;
}
/* @Return 0 on crash
 * @Return 1 if no need to write
 * @Return 2 if need to write 
 * it appends @result with the user file if
 * conf = 0 otherwise it appends @result with
 * the upstream file modified by env_merge_conf function*/
int env_compute(stralloc *result,sv_alltype *sv, uint8_t conf)
{
	int r, write = 1 ;
	size_t newlen = 0 ;
	char *name = keep.s + sv->cname.name ;
	stralloc dst = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	
	if (!env_resolve_conf(&dst,MYUID))
		log_warnu_return(LOG_EXIT_ZERO,"resolve source of configuration file") ;

	newlen = dst.len ;
	if (!auto_stra(&dst,name))
		log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	
	r = scan_mode(dst.s,S_IFREG) ;
	if (!r || conf > 2)
	{ 
		// copy config file from upstream in sysadmin
		if (!stralloc_copy(result,&sv->saenv))
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		write = 2 ;
		goto freed ;
	}
	else if (conf > 0)
	{
		dst.s[newlen] = 0 ;
		if (!file_readputsa(&salist,dst.s,name)) 
			log_warnusys_return(LOG_EXIT_ZERO,"read: ",dst.s,name) ;

		//merge config from upstream to sysadmin
		if (!env_merge_conf(result,&salist,&sv->saenv,conf))
			log_warnu_return(LOG_EXIT_ZERO,"merge environment file") ;
		write = 2 ;
		goto freed ;
	}
	dst.s[newlen] = 0 ;
	if (!file_readputsa(result,dst.s,name)) 
		log_warnusys_return(LOG_EXIT_ZERO,"read: ",dst.s,name) ;
	
	freed:
	stralloc_free(&dst) ;
	stralloc_free(&salist) ;
	
	return write ;
}

int env_clean_with_comment(stralloc *sa)
{
	ssize_t pos = 0, r ;
	char *end = 0, *start = 0 ;
	stralloc final = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;

	if (!sastr_split_string_in_nline(sa)) 
		log_warnu_return(LOG_EXIT_ZERO,"split environment value") ;

	for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
	{
		tmp.len = 0 ;
		if (!sastr_clean_string(&tmp,sa->s + pos))
			log_warnu_return(LOG_EXIT_ZERO,"clean environment string") ;
		/** keep a empty line between key=value pair and a comment */
		r = get_len_until(tmp.s,'=') ;
		end = r < 0 ? "\n" : "\n\n" ;
		start = r < 0 ? "" : "\n" ;
		
		if (tmp.s[0] == '#')
		{
			if (!sastr_rebuild_in_oneline(&tmp))
				log_warnu_return(LOG_EXIT_ZERO,"rebuild environment string in one line") ;
		}
		else
		{
			if (!environ_rebuild_line(&tmp))
				log_warnu_return(LOG_EXIT_ZERO,"rebuild environment line") ;
		}
				
		if (!auto_stra(&final,start,tmp.s,end))
			log_warn_return(LOG_EXIT_ZERO,"append stralloc") ;		
	}
	sa->len = 0 ;
	if (!auto_stra(sa,final.s))
		log_warnu_return(LOG_EXIT_ZERO,"store environment value") ;

	stralloc_free(&tmp) ;
	stralloc_free(&final) ;
	
	return 1 ;
}
