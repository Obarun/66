/* 
 * parser.c
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

#include <66/parser.h>

#include <string.h>
#include <stdio.h> //rename
#include <unistd.h> //chdir

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>
#include <oblibs/mill.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

/** return 1 if all good
 * return 0 on crash
 * return 2 on already enabled 
 * @svname do not contents the path of the frontend file*/
int parse_module(sv_alltype *sv_before,ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *svname,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t conf)
{
	log_trace("start parse process of module: ",svname) ;
	int r, err = 1, insta ;
	size_t in = 0 , pos = 0, inlen = 0, id, nid ;
	stralloc sdir = STRALLOC_ZERO ; // service dir
	stralloc list = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc svinsta = STRALLOC_ZERO ;
	
	/** should be always right */
	insta = instance_check(svname) ;
	if (!instance_splitname(&svinsta,svname,insta,SS_INSTANCE_NAME))
		log_warnu_return(LOG_EXIT_ZERO,"get instance name of: ",svname);
	
	if (!ss_resolve_module_path(&sdir,&tmp,svname,info->owner)) return 0 ;
	
	r = scan_mode(sdir.s,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; log_warnusys_return(LOG_EXIT_ZERO,"conflicting format of: ",sdir.s) ; }
	else if (!r)
	{
		if (!hiercopy(tmp.s,sdir.s))
			log_warnusys_return(LOG_EXIT_ZERO,"copy: ",tmp.s," to: ",sdir.s) ;
	}
	else
	{
		/** Must reconfigure all services of the module */
		if (force < 2)
		{
			log_warn("Ignoring module: ",svname," -- already configured") ;
			err = 2 ;
			goto make_deps ;
		}
		
		if (rm_rf(sdir.s) < 0)
			log_warnusys_return (LOG_EXIT_ZERO,"remove: ",sdir.s) ;
				
		if (!hiercopy(tmp.s,sdir.s))
			log_warnusys_return(LOG_EXIT_ZERO,"copy: ",tmp.s," to: ",sdir.s) ;
	}
	
	/** regex file content */
	if (!sastr_dir_get_recursive(&list,sdir.s,".configure",S_IFREG)) 
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;
	
	for (in = 0 ; in < list.len; in += strlen(list.s + in) + 1)
	{
		tmp.len = 0 ;
		char *str = list.s + in ;
		size_t len = strlen(str) ;
		char bname[len + 1] ;
		char dname[len + 1] ;
		if (!ob_basename(bname,str)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
		if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;
		
		log_trace("read service file of: ",dname,bname) ;
		if (!read_svfile(&tmp,bname,dname)) log_warnusys_return(LOG_EXIT_ZERO,"read file: ",str) ;

		pos = sv_before->type.module.start_infiles, inlen = sv_before->type.module.end_infiles ;
		for (; pos < inlen ; pos += strlen(keep.s + pos) + 1)
		{
			int all = 0, fpos = 0 ;
			char filename[512] = { 0 } ;
			char replace[512] = { 0 } ;
			char regex[512] = { 0 } ;
			char const *line = keep.s + pos ;
		
			if (strlen(line) >= 511) log_warn_return(LOG_EXIT_ZERO,"limit exceeded in service: ", svname) ;
			if ((line[0] != ':') || (get_sep_before(line + 1,':','=') < 0))
				log_warn_return(LOG_EXIT_ZERO,"bad format in line: ",line," of key @infiles field") ;
				
			fpos = regex_get_file_name(filename,line) ;

			if (fpos == -1)  log_warnu_return(LOG_EXIT_ZERO,"get filename of line: ",line) ;
			else if (fpos < 3) all = 1 ;
			
			if (!regex_get_replace(replace,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;

			if (!regex_get_regex(regex,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;
			
			if (obstr_equal(bname,filename) || all)
			{
				if (!sastr_replace_all(&tmp,replace,regex)) log_warnu_return(LOG_EXIT_ZERO,"replace: ",replace," by: ", regex," in file: ",str) ;
				if (!file_write_unsafe(dname,bname,tmp.s,tmp.len))
					log_warnusys_return(LOG_EXIT_ZERO,"write: ",dname,"/","filename") ;
			}
		}
	}
	
	/* regex directories name */
	if (!regex_replace(sv_before->type.module.iddir,sv_before->type.module.ndir,sdir.s,S_IFDIR)) return 0 ;
	/* regex files name */
	if (!regex_replace(sv_before->type.module.idfiles,sv_before->type.module.nfiles,sdir.s,S_IFREG)) return 0 ;
	/*  configure script */
	tmp.len = 0 ;
	if (!auto_stra(&tmp,sdir.s,"/.configure/configure")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	r = scan_mode(tmp.s,S_IFREG) ;
	if (r > 0)
	{
		stralloc oenv = STRALLOC_ZERO ;
		stralloc env = STRALLOC_ZERO ;
		/** environment is not mandatory */
		if (sv_before->opts[2] > 0)
		{
			/** sv_before->srconf is not set yet
			* we don't care, env_compute will find it.
			* env_compute return 2 in case of need to write */
			r = env_compute(&oenv,sv_before,conf) ;
			if (!r) log_warnu_return(LOG_EXIT_ZERO,"compute environment") ;
			if (!stralloc_0(&oenv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;	
			/** prepare for the environment merge */
				
			if (!environ_clean_envfile(&env,&oenv))
				log_warnu_return(LOG_EXIT_ZERO,"prepare environment") ;
			if (!stralloc_0(&oenv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;	
					
			if (!sastr_split_string_in_nline(&env))
				log_warnu_return(LOG_EXIT_ZERO,"rebuild environment") ;
		}
		size_t n = env_len((const char *const *)environ) + 1 + byte_count(env.s,env.len,'\0') ;
		char const *newenv[n + 1] ;
		if (!env_merge (newenv, n ,(const char *const *)environ,env_len((const char *const *)environ),env.s, env.len)) 
			log_warnusys_return(LOG_EXIT_ZERO,"build environment") ;
		
		int wstat ;
		pid_t pid ;
		size_t clen = sv_before->type.module.configure > 0 ? 1 : 0 ;
		char const *newargv[2 + clen] ;
		unsigned int m = 0 ;
		char pwd[sdir.len + 12] ;
		auto_strings(pwd,sdir.s,"/.configure") ;
		if (chdir(pwd) < 0) log_warnusys_return(LOG_EXIT_ZERO,"chdir to: ",pwd) ;
		
		newargv[m++] = tmp.s ;
		if (sv_before->type.module.configure > 0)
			newargv[m++] = keep.s + sv_before->type.module.configure ;
		newargv[m++] = 0 ;
		log_trace("launch script configure of module: ",svname) ;
		pid = child_spawn0(newargv[0],newargv,newenv) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"wait for: ",tmp.s) ;

		if (wstat) log_warnu_return(LOG_EXIT_ZERO,"run: ",tmp.s) ;
		tmp.len = 0 ;
		if (!auto_stra(&tmp,sdir.s,"/.configure")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (rm_rf(tmp.s) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"remove: ",tmp.s) ;
		
		stralloc_free(&oenv) ;
		stralloc_free(&env) ;
	}
	make_deps:
	/** get all services */
	tmp.len = 0 ;
	list.len = 0 ;
	if (!sastr_dir_get_recursive(&list,sdir.s,".configure",S_IFREG)) 
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;
	
	sdir.len = 0 ;
	
	/** remake the deps field */
	id = sv_before->cname.idga, nid = sv_before->cname.nga ;
	for (;nid; id += strlen(deps.s + id) + 1, nid--) {
		if (!stralloc_catb(&tmp,deps.s + id,strlen(deps.s + id) + 1)) 
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_catb(&sdir,deps.s + id,strlen(deps.s + id) + 1)) 
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	
	/** parse all services contained into the modules */
	for (pos = 0 ; pos < list.len ; pos += strlen(list.s + pos) + 1)
	{
		int svtype, insta ;
		char *sv = list.s + pos ;
		size_t len = strlen(sv) ;
		char bname[len + 1] ;
		char *pbname = bname ;
		
		if (!ob_basename(bname,sv))
			log_warnu_return(LOG_EXIT_ZERO,"find basename of: ",sv) ;
		
		char tinsta[len + svinsta.len + 1] ;
		char bname_insta[len + svinsta.len + 1] ;
		insta = get_len_until(bname,'@') ;
		if (insta > 0)
		{
			
			auto_strings(tinsta,sv,svinsta.s) ;
			sv = tinsta ;
			auto_strings(bname_insta,bname,svinsta.s) ;
			pbname = bname_insta ;
		}
			
		if (!parse_service_before(info,parsed_list,tree_list,sv,nbsv,sasv,force,conf))
			log_warnu_return(LOG_EXIT_ZERO,"parse: ",sv," from module: ",svname) ;
		
		/** always use the original name -> list.s + pos */
		svtype = get_svtype_from_file(list.s + pos) ;
		if (svtype == -1) log_warnu_return(LOG_EXIT_ZERO,"get svtype of: ",sv) ;

		if (!stralloc_catb(&sdir,pbname,strlen(pbname) + 1))
			log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

		if (svtype != TYPE_CLASSIC)
			if (!stralloc_catb(&tmp,pbname,strlen(pbname) + 1)) 
				log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	sv_before->cname.idga = deps.len ;
	sv_before->cname.nga = 0 ;
	for (pos = 0 ;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
	{
		stralloc_catb(&deps,tmp.s + pos,strlen(tmp.s + pos) + 1) ;
		sv_before->cname.nga++ ;
	}

	sv_before->cname.idcontents = deps.len ;
	for (pos = 0 ;pos < sdir.len ; pos += strlen(sdir.s + pos) + 1)
	{
		stralloc_catb(&deps,sdir.s + pos,strlen(sdir.s + pos) + 1) ;
		sv_before->cname.ncontents++ ;
	}
	
		
	stralloc_free(&sdir) ;
	stralloc_free(&list) ;
	stralloc_free(&tmp) ;
	stralloc_free(&svinsta) ;
	
	return err ;
}

/* helper */

/* 0 filename undefine
 * -1 system error 
 * should return at least 2 meaning :: no file define*/
int regex_get_file_name(char *filename,char const *str)
{
	int r ;
	size_t pos = 0 ;
	stralloc kp = STRALLOC_ZERO ;

	parse_mill_t MILL_GET_COLON = { 
	.open = ':', .close = ':',
	.skip = " \t\r", .skiplen = 3,
	.forceclose = 1,
	.inner.debug = "get_colon" } ;
	
	r = mill_element(&kp,str,&MILL_GET_COLON,&pos) ;
	if (r == -1) goto err ;
	
	auto_strings(filename,kp.s) ;
	
	stralloc_free(&kp) ;
	return pos ;
	err:
		stralloc_free(&kp) ;
		return -1 ;
}

int regex_get_replace(char *replace, char const *str)
{
	int pos = get_len_until(str,'=') ;
	if (!pos || pos == -1) return 0 ;
	char tmp[pos + 1] ;
	memcpy(tmp,str,pos) ;
	tmp[pos] = 0 ;
	auto_strings(replace,tmp) ;
	return 1 ;
}

int regex_get_regex(char *regex, char const *str)
{
	size_t len = strlen(str) ;
	int pos = get_len_until(str,'=') ;
	if (!pos || pos == -1) return 0 ;
	pos++ ; // remove '='
	char tmp[len + 1] ;
	memcpy(tmp,str + pos,len-pos) ;
	tmp[len-pos] = 0 ;
	auto_strings(regex,tmp) ;
	return 1 ;
}

int regex_replace(int id,unsigned int nid, char const *sdir,mode_t mode)
{
	stralloc list = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	size_t pos = id, len = nid, in ;
	if (!sastr_dir_get_recursive(&list,sdir,"",mode)) 
		log_warnusys_return(LOG_EXIT_ZERO,"get content of: ",sdir) ;
		
	pos = id, len = nid ;
	
	for (;len; pos += strlen(keep.s + pos) + 1,len--)
	{
		char *line = keep.s + pos ;
		char replace[512] = { 0 } ;
		char regex[512] = { 0 } ;
		if (!regex_get_replace(replace,line)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;
		if (!regex_get_regex(regex,line)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;
			
		for (in = 0 ; in < list.len; in += strlen(list.s + in) + 1)
		{
			tmp.len = 0 ;
			char *str = list.s + in ;
			size_t len = strlen(str) ;
			char dname[len + 1] ;
			if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;
		
			if (!sabasename(&tmp,str,len)) log_warnusys_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
			if (!stralloc_0(&tmp)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			
			if (!sastr_replace(&tmp,replace,regex)) log_warnu_return(LOG_EXIT_ZERO,"replace: ",replace," by: ", regex," in file: ",str) ;
			char new[len + tmp.len + 1] ;
			auto_strings(new,dname,tmp.s) ;
			/** do not try to rename the same directory */
			if (!obstr_equal(str,new))
				if (rename(str,new) < 0) 
					log_warnusys_return(LOG_EXIT_ZERO,"rename: ",str," by: ",new) ;
		}
	}
	stralloc_free(&tmp) ;
	stralloc_free(&list) ;
	return 1 ;
}
