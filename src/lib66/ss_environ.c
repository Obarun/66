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
#include <stdio.h>//rename
#include <errno.h>

#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>//atomic_symlink
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>//rm_rf

#include <66/constants.h>
#include <66/utils.h>
#include <66/environ.h>

int env_resolve_conf(stralloc *env,char const *svname, uid_t owner)
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
	if (!stralloc_cats(env,svname)) return 0 ;
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

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if symlink was updated and old version > new version
 * @Return 3 if symlink was updated and old version < new version */
int env_make_symlink(stralloc *dst,stralloc *old_dst,sv_alltype *sv,uint8_t conf)
{
	/** dst-> e.g /etc/66/conf/<service_name> */
	int r ;
	size_t dstlen = dst->len ;
	uint8_t format = 0, update_symlink = 0, write = 0 ;
	struct stat st ;
	stralloc saversion = STRALLOC_ZERO ;
	char *name = keep.s + sv->cname.name ;
	char *version = keep.s + sv->cname.version ;
	char old[dst->len + 5] ;//.old
	char dori[dst->len + 1] ;
	char dname[dst->len + 1] ;
	char current_version[dst->len + 1] ;
	char sym_version[dst->len + SS_SYM_VERSION_LEN + 1] ;
	auto_strings(sym_version,dst->s,SS_SYM_VERSION) ;

	/** dst -> /etc/66/conf/<service> */
	auto_strings(dori,dst->s) ;

	r = scan_mode(dst->s, S_IFDIR) ;
	/** enforce to pass to new format*/
	if (r == -1) {
		/** last time to pass to the new format. 0.6.0.0 or higher
		 * version will remove this check */
		auto_strings(old,dst->s,".old") ;
		if (rename(dst->s,old) == -1)
			log_warnusys_return(LOG_EXIT_ZERO,"rename: ",dst->s," to: ",old) ;
		format = 1 ;
	}

	/** first activation of the service. The symlink doesn't exist yet.
	 * In that case we create it else we check if it point to a file or
	 * a directory. File means old format, in this case we enforce to pass
	 * to the new one. */
	r = lstat(sym_version,&st) ;
	if (S_ISLNK(st.st_mode))
	{
		if (sarealpath(&saversion,sym_version) == -1)
			log_warn_return(LOG_EXIT_ZERO,"sarealpath of: ",sym_version) ;

		r = scan_mode(saversion.s,S_IFREG) ;
		if (r > 0)
		{
			/** /etc/66/conf/service/version/confile */
			if (!ob_dirname(dname,saversion.s))
				log_warn_return(LOG_EXIT_ZERO,"get basename of: ",saversion.s) ;
			dname[strlen(dname) -1] = 0 ;
			saversion.len = 0 ;
			if (!auto_stra(&saversion,dname))
				log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

			if (!ob_basename(current_version,dname))
				log_warn_return(LOG_EXIT_ZERO,"get basename of: ",dname) ;

			/** old format->point to a file instead of the directory,
			 * enforce to pass to new one */
			if (!atomic_symlink(dst->s,sym_version,"env_compute"))
				log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym_version," to: ",dst->s) ;
		}
		else
		{
			/** /etc/66/conf/service/version */
			if (!ob_basename(current_version,saversion.s))
				log_warn_return(LOG_EXIT_ZERO,"get basename of: ",saversion.s) ;
		}
		/** keep the path of the current symlink. env_compute need it
		 * to compare the two files. */
		if (sarealpath(old_dst,sym_version) == -1)
			log_warn_return(LOG_EXIT_ZERO,"sarealpath of: ",sym_version) ;
	}
	else
	{
		/** doesn't exist. First use of the service */
		update_symlink = 3 ;
		write = 1 ;
		if (!auto_stra(dst,"/",version))
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		goto create ;
	}

	/** 0.4.0.1 and previous 66 version doesn't set @version field as
	 * mandatory field. We need to check if we have a previous format. */
	r = version_scan(&saversion,current_version,SS_CONFIG_VERSION_NDOT) ;
	if (r == -1) log_warnu_return(LOG_EXIT_ZERO,"get version of: ",saversion.s) ;
	if (!r)
	{
		/** old format meaning /etc/66/conf/service/confile
		 * we consider it as old version */
		if (!auto_stra(dst,"/",current_version))
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

		if (!atomic_symlink(dst->s,sym_version,"env_compute"))
			log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym_version," to: ",dst->s) ;
	}

	r = version_cmp(current_version,version,SS_CONFIG_VERSION_NDOT) ;
	if (r == -2) log_warn_return(LOG_EXIT_ZERO,"compare version: ",current_version," vs: ",version) ;
	if (r == -1 || r == 1)
	{
		if (conf) {
			update_symlink = r == 1 ? 2 : 3 ;
			if (r == 1 && conf < 3)
				log_1_warn("merging configuration file to a previous version is not allowed -- keeps as it") ;

			dst->len = dstlen ;
			if (!auto_stra(dst,"/",version))
				log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		else
		{
			log_1_warn("configuration file version differ --  current: ",current_version," new: ",version) ;
			dst->len = dstlen ;
			if (!auto_stra(dst,SS_SYM_VERSION))
				log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
	}
	else
	{
		/** version is the same keep the previous symlink */
		dst->len = dstlen ;
		if (!auto_stra(dst,SS_SYM_VERSION))
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}

	saversion.len = 0 ;
	if (!auto_stra(&saversion,dori,"/",version,"/",name))
		log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

	r = scan_mode(saversion.s,S_IFREG) ;
	if (!r)
	{
		/** New version doesn't exist yet, we create it even
		 * if the symlink is not updated */
		saversion.len = 0 ;
		if (!auto_stra(&saversion,dori,"/",version))
			log_warn_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!dir_create_parent(saversion.s,0755))
			log_warnsys_return(LOG_EXIT_ZERO,"create directory: ",saversion.s) ;
		if (!write_env(name,&sv->saenv,saversion.s))
			log_warnu_return(LOG_EXIT_ZERO,"write environment at: ",saversion.s) ;
	}

	create:
	if (!dir_create_parent(dst->s,0755))
		log_warnsys_return(LOG_EXIT_ZERO,"create directory: ",dst->s) ;

	/** atomic_symlink check if exist
	 * if it doesn't exist, it create it*/
	if (update_symlink)
		if (!atomic_symlink(dst->s,sym_version,"env_compute"))
			log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym_version," to: ",dst->s) ;

	if (write)
		if (!write_env(name,&sv->saenv,dst->s))
			log_warnu_return(LOG_EXIT_ZERO,"write environment at: ",dst->s) ;

	/** last time guys! */
	if (format)
	{
		char tmp[dst->len + 1 + strlen(name) + 1] ;
		auto_strings(tmp,dst->s,"/",name) ;

		if (rename(old,tmp) == -1)
			log_warnusys_return(LOG_EXIT_ZERO,"rename: ",old," to: ",tmp) ;
		if (rm_rf(old) == -1)
			log_warnusys_return(LOG_EXIT_ZERO,"remove: ",old) ;
	}

	stralloc_free(&saversion) ;
	return update_symlink ? update_symlink : 1 ;
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
	uint8_t symlink_updated = 0 ;
	stralloc dst = STRALLOC_ZERO ;
	stralloc old_dst = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;

	if (!auto_stra(&dst,keep.s + sv->srconf))
		log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

	symlink_updated = env_make_symlink(&dst,&old_dst,sv,conf) ;
	if (!symlink_updated) return 0 ;

	if (!auto_stra(&dst,"/",keep.s + sv->cname.name))
		log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

	/** dst-> /etc/66/conf/service/version/confile */
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
		if ((symlink_updated == 3) && old_dst.len)
		{
			if (!auto_stra(&old_dst,"/",keep.s + sv->cname.name))
				log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

			if (!file_readputsa_g(&salist,old_dst.s))
				log_warnusys_return(LOG_EXIT_ZERO,"read: ",old_dst.s) ;
		}
		else
		{
			if (!file_readputsa_g(&salist,dst.s))
				log_warnusys_return(LOG_EXIT_ZERO,"read: ",dst.s) ;
		}

		//merge config from upstream to sysadmin
		if (!env_merge_conf(result,&salist,&sv->saenv,conf))
			log_warnu_return(LOG_EXIT_ZERO,"merge environment file") ;
		write = 2 ;
		goto freed ;
	}

	if (!file_readputsa_g(result,dst.s))
		log_warnusys_return(LOG_EXIT_ZERO,"read: ",dst.s) ;
	
	freed:
	stralloc_free(&dst) ;
	stralloc_free(&old_dst) ;
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

int env_find_current_version(stralloc *sa,char const *svconf)
{
	size_t svconflen = strlen(svconf) ;
	char tmp[svconflen + SS_SYM_VERSION_LEN + 1] ;
	auto_strings(tmp,svconf,SS_SYM_VERSION) ;
	if (sareadlink(sa,tmp) == -1) log_warnusys_return(LOG_EXIT_ZERO,"readlink: ",tmp) ;
	if (!stralloc_0(sa)) log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;
	return 1 ;
}
