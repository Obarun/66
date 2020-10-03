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
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>


#define SS_MODULE_CONFIG_DIR "/configure"
#define SS_MODULE_CONFIG_DIR_LEN (sizeof SS_MODULE_CONFIG_DIR - 1)
#define SS_MODULE_CONFIG_SCRIPT "configure"
#define SS_MODULE_CONFIG_SCRIPT_LEN (sizeof SS_MODULE_CONFIG_SCRIPT - 1)
#define SS_MODULE_SERVICE "/service"
#define SS_MODULE_SERVICE_LEN (sizeof SS_MODULE_SERVICE - 1)
#define SS_MODULE_SERVICE_INSTANCE "/service@"
#define SS_MODULE_SERVICE_INSTANCE_LEN (sizeof SS_MODULE_SERVICE_INSTANCE - 1)

static int check_dir(char const *src,char const *dir)
{
	int r ;
	size_t srclen = strlen(src) ;
	size_t dirlen = strlen(dir) ;

	char tsrc[srclen + dirlen + 1] ;
	auto_strings(tsrc,src,dir) ;

	r = scan_mode(tsrc,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; log_warnusys_return(LOG_EXIT_ZERO,"conflicting format of: ",tsrc) ; }
	if (!r)
	{
		if (!dir_create_parent(tsrc,0755))
			log_warnusys_return(LOG_EXIT_ZERO,"create directory: ",tsrc) ;
	}
	return 1 ;
}

static int get_list(stralloc *list, stralloc *sdir,size_t len, char const *svname, mode_t mode)
{
	sdir->len = len ;
	if (!auto_stra(sdir,SS_MODULE_SERVICE)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	if (!sastr_dir_get_recursive(list,sdir->s,"",mode))
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;

	sdir->len = len ;

	if (!auto_stra(sdir,SS_MODULE_SERVICE_INSTANCE)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	if (!sastr_dir_get_recursive(list,sdir->s,"",mode))
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;

	return 1 ;
}

static int rebuild_list(sv_alltype *sv_before,stralloc *list,stralloc *sv_all_type, stralloc *module_service)
{
	size_t pos, id, did, nid, dnid ;
	sv_alltype_ref sv ;
	id = sv_before->cname.idga, nid = sv_before->cname.nga ;
	for (;nid; id += strlen(deps.s + id) + 1, nid--)
	{
		char *deps_name = deps.s + id ;
		for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv) ; pos++)
		{
			sv = &genalloc_s(sv_alltype,&gasv)[pos] ;
			int type = sv->cname.itype == TYPE_MODULE ? 1 : 0 ;
			char *n = keep.s + sv->cname.name ;
			if (!strcmp(n,deps_name))
			{
				if (!stralloc_catb(list,keep.s + sv->src,strlen(keep.s + sv->src) + 1)) return 0 ;
				if (!sv->cname.nga) continue ;
				did = type ? sv->cname.idcontents : sv->cname.idga, dnid = type ? sv->cname.ncontents : sv->cname.nga ;
				for (;dnid; did += strlen(deps.s + did) + 1, dnid--)
				{
					if (sastr_cmp(list,deps.s + did) >= 0) continue ;
					if (!rebuild_list(sv,list,sv_all_type,module_service))
						log_warnu(LOG_EXIT_ZERO,"rebuild dependencies list of: ",deps.s + did) ;
				}
			}
		}
		if (sastr_cmp(sv_all_type,deps_name) == -1)
			if (!stralloc_catb(sv_all_type,deps_name,strlen(deps_name) + 1)) return 0 ;
		if (sastr_cmp(module_service,deps_name) == -1)
			if (!stralloc_catb(module_service,deps_name,strlen(deps_name) + 1)) return 0 ;
	}

	return 1 ;
}

/** return 1 on success
 * return 0 on failure
 * return 2 on already enabled 
 * @svname do not contents the path of the frontend file*/

int parse_module(sv_alltype *sv_before,ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *svname,char const *src_frontend,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t conf,uint8_t import)
{
	log_trace("start parse process of module: ",svname) ;
	int r, err = 1, insta = -1, svtype = -1, from_ext_insta = 0, already_parsed = 0 ;
	size_t pos = 0, id, nid, newlen ;
	stralloc sdir = STRALLOC_ZERO ; // service dir
	stralloc list = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc moduleinsta = STRALLOC_ZERO ;
	stralloc addonsv = STRALLOC_ZERO ;
	
	/** should be always right,
	 * be paranoid and check it */
	insta = instance_check(svname) ;
	if (insta <= 0)
		log_warn_return(LOG_EXIT_ZERO,"invalid module instance name: ",svname);
	
	if (!ss_resolve_module_path(&sdir,&tmp,svname,src_frontend,info->owner)) return 0 ;

	/** check mandatory directories:
	 * module/module_name, module/module_name/{configure,service,service@} */
	if (!check_dir(tmp.s,"")) return 0 ;
	if (!check_dir(tmp.s,SS_MODULE_CONFIG_DIR)) return 0 ;
	if (!check_dir(tmp.s,SS_MODULE_SERVICE)) return 0 ;
	if (!check_dir(tmp.s,SS_MODULE_SERVICE_INSTANCE)) return 0 ;

	newlen = sdir.len ;

	char permanent_sdir[sdir.len + 2] ;
	auto_strings(permanent_sdir,sdir.s,"/") ;

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
			log_warn("skip configuration of the module: ",svname," -- already configured") ;
			err = 2 ;
			goto make_deps ;
		}
		
		if (rm_rf(sdir.s) < 0)
			log_warnusys_return (LOG_EXIT_ZERO,"remove: ",sdir.s) ;
				
		if (!hiercopy(tmp.s,sdir.s))
			log_warnusys_return(LOG_EXIT_ZERO,"copy: ",tmp.s," to: ",sdir.s) ;
	}

	/** regex file content */
	list.len = 0 ;

	if (!get_list(&list,&sdir,newlen,svname,S_IFREG)) return 0 ;
	if (!regex_replace(&list,sv_before,svname)) return 0 ;

	/* regex directories name */
	if (!get_list(&list,&sdir,newlen,svname,S_IFDIR)) return 0 ;
	if (!regex_rename(&list,sv_before->type.module.iddir,sv_before->type.module.ndir,sdir.s)) return 0 ;

	/* regex files name */
	list.len = 0 ;

	if (!get_list(&list,&sdir,newlen,svname,S_IFREG)) return 0 ;
	if (!regex_rename(&list,sv_before->type.module.idfiles,sv_before->type.module.nfiles,sdir.s)) return 0 ;

	/* launch configure script */
	if (!regex_configure(sv_before,info,permanent_sdir,svname,conf,import)) return 0 ;

	make_deps:

	tmp.len = 0 ;
	list.len = 0 ;

	if (!auto_stra(&tmp,permanent_sdir,SS_MODULE_SERVICE + 1))
		log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	/** get all services */
	if (!sastr_dir_get_recursive(&list,tmp.s,"",S_IFREG))
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;

	/** add addon service */
	if (sv_before->type.module.naddservices > 0)
	{
		id = sv_before->type.module.idaddservices, nid = sv_before->type.module.naddservices ;
		for (;nid; id += strlen(keep.s + id) + 1, nid--)
		{
			char *name = keep.s + id ;
			if (ss_resolve_src_path(&list,name,info->owner,0) < 1)
					log_warnu_return(LOG_EXIT_ZERO,"resolve source path of: ",name) ;
		}
	}

	tmp.len = 0 ;
	sdir.len = 0 ;

	/** remake the deps field
	 * incoporate the module service deps inside the list to parse
	 * and each dependency of each module service dependency.
	 * Do it recursively. */
	if (!rebuild_list(sv_before,&list,&tmp,&sdir))
		log_warnu(LOG_EXIT_ZERO,"rebuild dependencies list of; ",svname) ;

	/** parse all services of the modules */
	for (pos = 0 ; pos < list.len ; pos += strlen(list.s + pos) + 1)
	{
		insta = 0, svtype = -1 , from_ext_insta = 0 , already_parsed = 0 ;
		char *sv = list.s + pos ;
		size_t len = strlen(sv) ;
		char bname[len + 1] ;
		char *pbname = 0 ;

		addonsv.len = 0 ;

		if (sastr_cmp(parsed_list,sv) >= 0)
		{
			/** already parsed ? */
			size_t idx = 0 ;
			for (; idx < genalloc_len(sv_alltype,&gasv) ; idx++)
			{
				char *name = keep.s + genalloc_s(sv_alltype,&gasv)[idx].src ;

				if (!strcmp(name,sv))
				{
					sv = name ;
					break ;
				}
			}
			already_parsed = 1 ;
		}

		if (!ob_basename(bname,sv))
			log_warnu_return(LOG_EXIT_ZERO,"find basename of: ",sv) ;

		pbname = bname ;

		/** detect cyclic call. Sub-module cannot call it itself*/
		if (!strcmp(svname,bname))
			log_warn_return(LOG_EXIT_ZERO,"cyclic call detected -- ",svname," call ",bname) ;

		insta = instance_check(bname) ;
		if (!insta) log_warn_return(LOG_EXIT_ZERO,"invalid instance name: ",sv) ;
		if (insta > 0)
		{
			/** we can't know the origin of the instanciated service.
			 * Search first at service@ directory, if it not found
			 * pass through the classic ss_resolve_src_path() */

			pbname = bname ;
			if (!already_parsed)
			{
				int found = 0 ;
				size_t l = strlen(permanent_sdir) ;
				char tmp[l + SS_MODULE_SERVICE_INSTANCE_LEN + 2] ;
				auto_strings(tmp,permanent_sdir,SS_MODULE_SERVICE_INSTANCE + 1,"/") ;

				r = ss_resolve_src(&addonsv,pbname,tmp,&found) ;

				if (r == -1) log_warnusys_return(LOG_EXIT_ZERO,"parse source directory: ",tmp) ;
				if (!r)
				{
					if (ss_resolve_src_path(&addonsv,pbname,info->owner,0) < 1)
						log_warnu_return(LOG_EXIT_ZERO,"resolve source path of: ",pbname) ;
				}
				sv = addonsv.s ;
			}
			from_ext_insta++ ;
			len = strlen(sv) ;
		}

		if (!already_parsed)
			if (!parse_service_before(info,parsed_list,tree_list,sv,nbsv,sasv,force,conf,import,0,permanent_sdir))
				log_warnu_return(LOG_EXIT_ZERO,"parse: ",sv," from module: ",svname) ;

		char ext_insta[len + 1] ;
		if (from_ext_insta) {
			size_t len = strlen(sv) ;
			r = get_rlen_until(sv,'@',len) + 1 ;
			size_t newlen = len - (len - r) ;
			auto_strings(ext_insta,sv) ;
			ext_insta[newlen] = 0 ;
			sv = ext_insta ;
		}

		/** we want the configuration file for each service inside
		 * the configuration directory of the module.
		 * In case of sub-module, we skip it.
		 * Also, we skip every dependency of the sub-module,
		 * each module contains its own services.*/
		char *version = keep.s + sv_before->cname.version ;
		{
			stralloc tmpenv = STRALLOC_ZERO ;
			/** 512 is the maximum of services set by default
			 * that can be supervised by s6-svscan. It should be large
			 * enough for the majority of the cases */
			size_t pos, spos, id, nid, idmodule[512] = { 0 } ;
			sv_alltype_ref svref ;

			if (!env_resolve_conf(&tmpenv,svname,info->owner))
				log_warnu_return(LOG_EXIT_ZERO,"get path of the configuration file") ;

			if (!auto_stra(&tmpenv,"/")) log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

			/** search first for all sub-modules */
			for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv);pos++)
			{
				if (genalloc_s(sv_alltype,&gasv)[pos].cname.itype == TYPE_MODULE)
				{
					idmodule[pos] = 1 ;

					svref = &genalloc_s(sv_alltype,&gasv)[pos] ;

					id = svref->cname.idcontents, nid = svref->cname.ncontents ;
					for (;nid; id += strlen(deps.s + id) + 1, nid--)
					{
						char *name = deps.s + id ;

						for (spos = 0 ; spos < genalloc_len(sv_alltype,&gasv);spos++)
							if (!strcmp(name,keep.s + genalloc_s(sv_alltype,&gasv)[spos].cname.name))
								idmodule[spos] = 1 ;
					}
				}
			}

			for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv);pos++)
			{
				if (!genalloc_s(sv_alltype,&gasv)[pos].opts[2] ||
				idmodule[pos]) continue ;
				char *n = keep.s + genalloc_s(sv_alltype,&gasv)[pos].cname.name ;

				if (!strcmp(n,bname))
				{
					genalloc_s(sv_alltype,&gasv)[pos].srconf = keep.len ;
					if (!auto_stra(&tmpenv,version,"/",bname)) log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

					if (!stralloc_catb(&keep,tmpenv.s,strlen(tmpenv.s) + 1))
						log_warn_return(LOG_EXIT_ZERO,"stralloc") ;
					break ;
				}
			}
			stralloc_free(&tmpenv) ;
		}

		svtype = get_svtype_from_file(sv) ;
		if (svtype == -1) log_warnu_return(LOG_EXIT_ZERO,"get svtype of: ",sv) ;

		if (sastr_cmp(&sdir,pbname) == -1)
			if (!stralloc_catb(&sdir,pbname,strlen(pbname) + 1))
				log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

		if (sastr_cmp(&tmp,pbname) == -1)
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

	tmp.len = 0 ;
	if (!auto_stra(&tmp,permanent_sdir,SS_MODULE_CONFIG_DIR + 1))
		log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (rm_rf(tmp.s) < 0)
		log_warnusys_return(LOG_EXIT_ZERO,"remove: ",tmp.s) ;

	stralloc_free(&sdir) ;
	stralloc_free(&list) ;
	stralloc_free(&tmp) ;
	stralloc_free(&moduleinsta) ;
	stralloc_free(&addonsv) ;

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

int regex_replace(stralloc *list,sv_alltype *sv_before,char const *svname)
{
	int r ;
	size_t in = 0, pos, inlen ;

	stralloc tmp = STRALLOC_ZERO ;

	for (; in < list->len; in += strlen(list->s + in) + 1)
	{
		tmp.len = 0 ;
		char *str = list->s + in ;
		size_t len = strlen(str) ;
		char bname[len + 1] ;
		char dname[len + 1] ;
		if (!ob_basename(bname,str)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
		if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;

		log_trace("read service file of: ",dname,bname) ;
		r = read_svfile(&tmp,bname,dname) ;
		if (!r) log_warnusys_return(LOG_EXIT_ZERO,"read file: ",str) ;
		if (r == -1) continue ;
		
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
	stralloc_free(&tmp) ;

	return 1 ;
}

int regex_rename(stralloc *list, int id, unsigned int nid, char const *sdir)
{
	stralloc tmp = STRALLOC_ZERO ;
	size_t pos = id, len = nid, in ;

	pos = id, len = nid ;

	for (;len; pos += strlen(keep.s + pos) + 1,len--)
	{
		char *line = keep.s + pos ;
		char replace[512] = { 0 } ;
		char regex[512] = { 0 } ;
		if (!regex_get_replace(replace,line)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;
		if (!regex_get_regex(regex,line)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;

		for (in = 0 ; in < list->len; in += strlen(list->s + in) + 1)
		{
			tmp.len = 0 ;
			char *str = list->s + in ;
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
			{
				log_trace("rename: ",str," to: ",new) ;
				if (rename(str,new) == -1)
					log_warnusys_return(LOG_EXIT_ZERO,"rename: ",str," to: ",new) ;
			}
		}
	}
	stralloc_free(&tmp) ;

	return 1 ;
}

int regex_configure(sv_alltype *sv_before,ssexec_t *info, char const *module_dir,char const *module_name,uint8_t conf,uint8_t import)
{
	int wstat, r ;
	pid_t pid ;
	size_t clen = sv_before->type.module.configure > 0 ? 1 : 0 ;
	size_t module_dirlen = strlen(module_dir), n ;
	size_t pos = 0 ;
	stralloc oenv = STRALLOC_ZERO ;
	stralloc env = STRALLOC_ZERO ;
	stralloc version = STRALLOC_ZERO ;
	stralloc salink = STRALLOC_ZERO ;

	char const *newargv[2 + clen] ;
	unsigned int m = 0 ;

	char pwd[module_dirlen + SS_MODULE_CONFIG_DIR_LEN + 1] ;
	auto_strings(pwd,module_dir,SS_MODULE_CONFIG_DIR + 1) ;

	char config_script[module_dirlen + SS_MODULE_CONFIG_DIR_LEN + 1 + SS_MODULE_CONFIG_SCRIPT_LEN + 1] ;
	auto_strings(config_script,module_dir,SS_MODULE_CONFIG_DIR + 1,"/",SS_MODULE_CONFIG_SCRIPT) ;

	r = scan_mode(config_script,S_IFREG) ;
	if (r > 0)
	{
		/** export ssexec_t info value on the environment */
		{
			char owner[UID_FMT];
			owner[uid_fmt(owner,info->owner)] = 0 ;
			char verbo[UINT_FMT];
			verbo[uid_fmt(verbo,VERBOSITY)] = 0 ;
			auto_stra(&env,"MOD_NAME=",module_name,"\n") ;
			auto_stra(&env,"MOD_BASE=",info->base.s,"\n") ;
			auto_stra(&env,"MOD_LIVE=",info->live.s,"\n") ;
			auto_stra(&env,"MOD_TREE=",info->tree.s,"\n") ;
			auto_stra(&env,"MOD_SCANDIR=",info->scandir.s,"\n") ;
			auto_stra(&env,"MOD_TREENAME=",info->treename.s,"\n") ;
			auto_stra(&env,"MOD_OWNER=",owner,"\n") ;
			auto_stra(&env,"MOD_COLOR=",info->opt_color ? "1" : "0","\n") ;
			auto_stra(&env,"MOD_VERBOSITY=",verbo,"\n") ;
			auto_stra(&env,"MOD_MODULE_DIR=",module_dir,"\n") ;
			auto_stra(&env,"MOD_SKEL_DIR=",SS_SKEL_DIR,"\n") ;
			auto_stra(&env,"MOD_SERVICE_SYSDIR=",SS_SERVICE_SYSDIR,"\n") ;
			auto_stra(&env,"MOD_SERVICE_ADMDIR=",SS_SERVICE_ADMDIR,"\n") ;
			auto_stra(&env,"MOD_SERVICE_ADMCONFDIR=",SS_SERVICE_ADMCONFDIR,"\n") ;
			auto_stra(&env,"MOD_MODULE_SYSDIR=",SS_MODULE_SYSDIR,"\n") ;
			auto_stra(&env,"MOD_MODULE_ADMDIR=",SS_MODULE_ADMDIR,"\n") ;
			auto_stra(&env,"MOD_SCRIPT_SYSDIR=",SS_SCRIPT_SYSDIR,"\n") ;
			auto_stra(&env,"MOD_USER_DIR=",SS_USER_DIR,"\n") ;
			auto_stra(&env,"MOD_SERVICE_USERDIR=",SS_SERVICE_USERDIR,"\n") ;
			auto_stra(&env,"MOD_SERVICE_USERCONFDIR=",SS_SERVICE_USERCONFDIR,"\n") ;
			auto_stra(&env,"MOD_MODULE_USERDIR=",SS_MODULE_USERDIR,"\n") ;
			auto_stra(&env,"MOD_SCRIPT_USERDIR=",SS_SCRIPT_USERDIR,"\n") ;
		}
		/** environment is not mandatory */
		if (sv_before->opts[2] > 0)
		{
			char *dst = keep.s + sv_before->srconf ;
			char *name = keep.s + sv_before->cname.name ;
			size_t dstlen = strlen(dst) ;
			char tdst[dstlen + SS_SYM_VERSION_LEN + 1] ;
			auto_strings(tdst,dst,SS_SYM_VERSION) ;
			uint8_t importless = 1 ;

			if (import)
			{
				r = env_find_current_version(&version,keep.s + sv_before->srconf) ;
				/** not a fatal error, the previous version may not exist
				 * at the first activation of the service. Anyway, warn the user */
				if (!r)
				{
					log_warn("import asked but cannot find the previous version for service: ",name) ;
					importless = 0 ;
				}
				else
				{
					char bname[version.len + 1] ;
					if (!ob_basename(bname,version.s))
						log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",version.s) ;

					r = version_cmp(bname,keep.s + sv_before->cname.version,SS_CONFIG_VERSION_NDOT) ;
					if (!r) { importless = 0 ; }
					else
					{
						version.len = 0 ;
						if (!auto_stra(&version,bname,",",keep.s + sv_before->cname.version))
							log_warnu_return(LOG_EXIT_ZERO,"stralloc") ;
					}
				}
			}
			/** env_compute return 2 in case if we need to write the file */
			r = env_compute(&oenv,sv_before,conf) ;
			if (!r) log_warnu_return(LOG_EXIT_ZERO,"compute environment") ;

			if (sareadlink(&salink, tdst) == -1)
				log_warnusys_return(LOG_EXIT_ZERO,"read link of: ",tdst) ;

			if (!stralloc_0(&salink))
				log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

			if (r == 2)
			{
				if (!write_env(name,&oenv,salink.s))
					log_warnu_return(LOG_EXIT_ZERO,"write environment") ;
			}
			if (import && importless)
			{
				int nargc = 9 + (info->opt_color ? 1 : 0) ;
				char const *newargv[nargc] ;
				char fmt[UINT_FMT] ;
				fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;

				newargv[m++] = SS_BINPREFIX "66-env" ;
				newargv[m++] = "-v" ;
				newargv[m++] = fmt ;
				if (info->opt_color)
					newargv[m++] = "-z" ;
				newargv[m++] = "-t" ;
				newargv[m++] = info->treename.s ;
				newargv[m++] = "-i" ;
				newargv[m++] = version.s ;
				newargv[m++] = name ;
				newargv[m++] = 0 ;

				pid = child_spawn0(newargv[0],newargv,(char const *const *)environ) ;
				if (waitpid_nointr(pid,&wstat, 0) < 0)
					log_warnu("wait for: ",newargv[0]) ;

				if (wstat)
					log_warnu("import previous configuration files") ;
			}
			/** Reads all file from the directory */
			version.len = 0 ;
			if (!sastr_dir_get(&version,salink.s,name,S_IFREG))
				log_warnu_return(LOG_EXIT_ZERO,"get environment files from: ",salink.s) ;

			for (; pos < version.len ; pos += strlen(version.s) + 1)
				if (!file_readputsa(&oenv,salink.s,version.s + pos))
					log_warnusys(LOG_EXIT_ZERO,"read file: ",salink.s,"/",version.s + pos) ;

			/** prepare for the environment merge */
			if (oenv.len) {
				if (!environ_clean_envfile(&env,&oenv))
					log_warnu_return(LOG_EXIT_ZERO,"prepare environment") ;
				if (!stralloc_0(&env)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				env.len-- ;
			}
		}
		if (!sastr_split_string_in_nline(&env))
			log_warnu_return(LOG_EXIT_ZERO,"rebuild environment") ;

		n = env_len((const char *const *)environ) + 1 + byte_count(env.s,env.len,'\0') ;
		char const *newenv[n + 1] ;
		if (!env_merge (newenv, n ,(const char *const *)environ,env_len((const char *const *)environ),env.s, env.len))
			log_warnusys_return(LOG_EXIT_ZERO,"build environment") ;

		if (chdir(pwd) < 0) log_warnusys_return(LOG_EXIT_ZERO,"chdir to: ",pwd) ;
		m = 0 ;
		newargv[m++] = config_script ;
		if (sv_before->type.module.configure > 0)
			newargv[m++] = keep.s + sv_before->type.module.configure ;
		newargv[m++] = 0 ;
		log_info("launch script configure of module: ",module_name) ;
		pid = child_spawn0(newargv[0],newargv,newenv) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"wait for: ",config_script) ;
		if (wstat) log_warnu_return(LOG_EXIT_ZERO,"run: ",config_script) ;
	}
	stralloc_free(&oenv) ;
	stralloc_free(&env) ;
	stralloc_free(&version) ;
	stralloc_free(&salink) ;

	return 1 ;
}
