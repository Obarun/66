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
#include <stdlib.h> //setenv

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

int parse_service_before(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force)
{
	log_trace("start parse process of service: ",sv) ;
	int insta ;
	uint8_t exist = 0 ;
	size_t svlen = strlen(sv), svsrclen, svnamelen ;
	char svname[svlen + 1], svsrc[svlen + 1] ; 
	if (!ob_basename(svname,sv)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",sv) ;
	if (!ob_dirname(svsrc,sv)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",sv) ;
	svsrclen = strlen(svsrc) ;
	svnamelen = strlen(svname) ;
	char svpath[svsrclen + svnamelen + 1] ;
	if (scan_mode(sv,S_IFDIR) == 1) return 1 ;
	char tree[info->tree.len + SS_SVDIRS_LEN + 1] ;
	auto_strings(tree,info->tree.s,SS_SVDIRS) ;
	
	int r = parse_service_check_enabled(tree,svname,force,&exist) ;
	if (!r) goto freed ;
		
	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	sasv->len = 0 ;

	insta = instance_check(svname) ;
	if (!insta) 
	{
		log_warn_return(LOG_EXIT_ZERO, "invalid instance name: ",svname) ;
	}
	else if (insta > 0)
	{
		stralloc tmp = STRALLOC_ZERO ;
		instance_splitname(&tmp,svname,insta,SS_INSTANCE_TEMPLATE) ;
		if (!read_svfile(sasv,tmp.s,svsrc)) return 0 ;
		stralloc_free(&tmp) ;
	}	
	else if (!read_svfile(sasv,svname,svsrc)) return 0 ;
	
	if (!get_svtype(&sv_before,sasv->s)) 
		log_warn_return (LOG_EXIT_ZERO,"invalid value for key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_TYPE)," in service file: ",svsrc,svname) ;
	
	if (insta > 0)
	{
		if (!instance_create(sasv,svname,SS_INSTANCE,insta))
			log_warn_return(LOG_EXIT_ZERO,"create instance service: ",svname) ;
		
		/** ensure that we have an empty line at the end of the string*/
		if (!stralloc_cats(sasv,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(sasv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}

	memcpy(svpath,svsrc,svsrclen) ;
	memcpy(svpath + svsrclen,svname,svnamelen) ;
	svpath[svsrclen + svnamelen] = 0 ;

	if (sastr_cmp(parsed_list,svpath) >= 0)
	{
		log_warn("Ignoring: ",sv," service: already parsed") ;
		sasv->len = 0 ;
		sv_alltype_free(&sv_before) ;
		goto freed ;
	}
	if (!parser(&sv_before,sasv,svname,sv_before.cname.itype)) return 0 ;

	/** keep the name set by user
	 * uniquely for instantiated service
	 * The name must contain the template string */

	if (insta > 0 && sv_before.cname.name >= 0 )
	{
		stralloc sainsta = STRALLOC_ZERO ;
		stralloc name = STRALLOC_ZERO ;
		if (!stralloc_cats(&name,keep.s + sv_before.cname.name)) goto err ;
		
		if (!instance_splitname(&sainsta,svname,insta,SS_INSTANCE_TEMPLATE)) goto err ;
		if (sastr_find(&name,sainsta.s) == -1)
		{
			log_warn("invalid instantiated service name: ", keep.s + sv_before.cname.name) ;
			goto err ;
		}
		stralloc_free(&sainsta) ;
		stralloc_free(&name) ;
		goto add ;
		err:
			stralloc_free(&sainsta) ;
			stralloc_free(&name) ;
			return 0 ;
	}
	else
	{
		sv_before.cname.name = keep.len ;
		if (!stralloc_catb(&keep,svname,svnamelen + 1)) return 0 ;
	}
	add:
	if (sv_before.cname.itype == TYPE_MODULE)
	{
		r = parse_module(&sv_before,svname,info->owner,force) ;
		if (!r) return 0 ;
		else if (r == 2)
		{
			sasv->len = 0 ;
			sv_alltype_free(&sv_before) ;
			goto deps ;
		}
		if (force > 1)
		{
			int wstat ;
			pid_t pid ;
			int color = log_color == &log_color_enable ? 1 : 0 ; 
			char const *newargv[9 + color + 1] ;
			unsigned int m = 0 ;
			char fmt[UINT_FMT] ;
			fmt[uint_fmt(fmt,VERBOSITY)] = 0 ;

			newargv[m++] = SS_BINPREFIX "66-disable" ;
			newargv[m++] = "-v" ;
			newargv[m++] = fmt ;
			if (color)
				newargv[m++] = "-z" ;
			newargv[m++] = "-l" ;
			newargv[m++] = info->live.s ;
			newargv[m++] = "-t" ;
			newargv[m++] = info->treename.s ;
			newargv[m++] = svname ;
			newargv[m++] = 0 ;
			
			pid = child_spawn0(newargv[0],newargv,(const char *const *)environ) ;
			if (waitpid_nointr(pid,&wstat, 0) < 0)
				log_warnusys_return (LOG_EXIT_ZERO,"wait for: 66-disable") ;
			/** may not be enabled yet, don't crash if it's the case */
			if (wstat && WEXITSTATUS(wstat) != LOG_EXIT_USER) log_warnu_return(LOG_EXIT_ZERO,"disable module: ",svname) ;
		}
	}
	deps:
	if (!parse_add_service(parsed_list,&sv_before,svpath,nbsv,info->owner)) return 0 ;
	
	if ((sv_before.cname.itype > TYPE_CLASSIC && force > 1) || !exist)
	{
		if (!parse_service_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,KEY_MAIN_OPTSDEPS)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,KEY_MAIN_EXTDEPS)) return 0 ;
	}
	freed:
	return 1 ;
}

/** return 1 if all good
 * return 0 on crash
 * return 2 on already enabled */
int parse_module(sv_alltype *sv_before,char const *svname,uid_t owner,uint8_t force)
{
	log_trace("start parse process of module: ",svname) ;
	int r, insta, err = 1 ;
	size_t in = 0 , pos = 0, len = 0 ;
	stralloc mdir = STRALLOC_ZERO ; // module dir
	stralloc sdir = STRALLOC_ZERO ; // service dir
	stralloc list = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc sainsta = STRALLOC_ZERO ; // SS_INSTANCE_NAME
	
	if (!ss_resolve_module_path(&sdir,&mdir,svname,owner)) return 0 ;
	
	/** keep instance name */
	insta = instance_check(svname) ;
	if (!instance_splitname(&sainsta,svname,insta,SS_INSTANCE_NAME))
		log_warnu_return(LOG_EXIT_ZERO,"get instance name") ;
	
	r = scan_mode(sdir.s,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; log_warnusys_return(LOG_EXIT_ZERO,"conflicting format of: ",sdir.s) ; }
	else if (!r)
	{
		if (!hiercopy(mdir.s,sdir.s))
			log_warnusys_return(LOG_EXIT_ZERO,"copy: ",mdir.s," to: ",sdir.s) ;
	}
	else
	{
		if (force < 2)
		{
			log_warn("Ignoring module: ",svname," -- already configured") ;
			err = 2 ;
			goto make_deps ;
		}
		
		if (rm_rf(sdir.s) < 0)
			log_warnusys_return (LOG_EXIT_ZERO,"remove: ",sdir.s) ;
				
		if (!hiercopy(mdir.s,sdir.s))
			log_warnusys_return(LOG_EXIT_ZERO,"copy: ",mdir.s," to: ",sdir.s) ;
	}
	
	/** regex file content */
	if (!sastr_dir_get_recursive(&list,sdir.s,".configure",S_IFREG)) 
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;
	
	pos = sv_before->type.module.start_infiles, len = sv_before->type.module.end_infiles ;
	for (;pos < len; pos += strlen(keep.s + pos) + 1)
	{
		int all = 0 ; int fpos = 0 ;
		char filename[512] = { 0 } ;
		char replace[512] = { 0 } ;
		char regex[512] = { 0 } ;
		char const *line = keep.s + pos ;
	
		if (strlen(line) >= 511) log_warn_return(LOG_EXIT_ZERO,"limit exceeded in service: ", svname) ;
		if ((line[0] != ':') || (get_sep_before(line + 1,':','=') < 0))
			log_warn_return(LOG_EXIT_ZERO,"bad format in line: ",line," of key @infiles field") ;
			
		fpos = regex_get_file_name(filename,line) ;

		if (fpos == -1)  log_warnu_return(LOG_EXIT_ZERO,"file name of line: ",line) ;
		else if (fpos < 3) all = 1 ;
		
		if (!regex_get_replace(replace,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;

		if (!regex_get_regex(regex,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;

		for (in = 0 ; in < list.len; in += strlen(list.s + in) + 1)
		{
			tmp.len = 0 ;
			char *str = list.s + in ;
			size_t len = strlen(str) ;
			char bname[len + 1] ;
			char dname[len + 1] ;
			if (!ob_basename(bname,str)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
			if (obstr_equal(bname,filename) || all)
			{
				if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;
				if (!read_svfile(&tmp,bname,dname)) log_warnusys_return(LOG_EXIT_ZERO,"read file: ",str) ;
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

		pid = child_spawn0(newargv[0],newargv,(const char *const *)environ) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"wait for: ",sdir.s) ;

		if (wstat) log_warnu_return(LOG_EXIT_ZERO,"run: ",sdir.s) ;
		tmp.len = 0 ;
		if (!auto_stra(&tmp,sdir.s,"/.configure")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (rm_rf(tmp.s) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"remove: ",tmp.s) ;
	}
	make_deps:
	/** get all services */
	list.len = 0 ;
	if (!sastr_dir_get_recursive(&list,sdir.s,".configure",S_IFREG)) 
		log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;
	
	/** remake the deps field */
	size_t id = sv_before->cname.idga, nid = sv_before->cname.nga ;
	sv_before->cname.idga = deps.len ;
	sv_before->cname.nga = 0 ;
	for (;nid; id += strlen(deps.s + id) + 1, nid--)
	{
		char *name = deps.s + id ;
		if (!stralloc_catb(&deps,name,strlen(deps.s + id) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ; ;
		sv_before->cname.nga++ ;
	}
	
	for (pos = 0 ; pos < list.len ; pos += strlen(list.s + pos) + 1)
	{
		char *name = list.s + pos ;
		char bname[len + sainsta.len + 1] ;
		if (!ob_basename(bname,name)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",name) ;
		insta = get_len_until(bname,'@') ;
		insta++ ; // keep '@'
		if (insta > 0)
		{
			auto_string_from(bname,insta,sainsta.s) ;
		}
		if (!stralloc_catb(&deps,bname,strlen(bname) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		sv_before->cname.nga++ ;
	}
	
	stralloc_free(&mdir) ;
	stralloc_free(&sdir) ;
	stralloc_free(&list) ;
	stralloc_free(&tmp) ;
	stralloc_free(&sainsta) ;
	
	return err ;
}

int parse_service_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force)
{
	int r ;
	char *dname = 0 ;
	stralloc newsv = STRALLOC_ZERO ;

	if (sv_before->cname.nga)
	{
		size_t id = sv_before->cname.idga, nid = sv_before->cname.nga ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			newsv.len = 0 ;
			if (sv_before->cname.itype != TYPE_BUNDLE)
			{
				log_trace("service: ",sv, " depends on: ",deps.s+id) ;
			}else log_trace("bundle: ",sv, " contents: ",deps.s+id," as service") ;
			dname = deps.s + id ;
			r = ss_resolve_src_path(&newsv,dname,info->owner) ;
			if (r < 1) goto err ;//don't warn here, the ss_revolve_src_path already warn user

			if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force)) goto err ;
		}
	}
	else log_trace(sv,": haven't dependencies") ;
	stralloc_free(&newsv) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		return 0 ;
}

int parse_service_opts_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list,sv_alltype *sv_before,char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force, uint8_t mandatory)
{
	int r ;
	stralloc newsv = STRALLOC_ZERO ;
		
	size_t pos = 0 , baselen = strlen(info->base.s) + SS_SYSTEM_LEN ;
	uint8_t found = 0, ext = 0 ;
	char *optname = 0 ;
	char btmp[baselen + 1] ;
	auto_strings(btmp,info->base.s,SS_SYSTEM) ;
	
	int idref = sv_before->cname.idopts ;
	unsigned int nref = sv_before->cname.nopts ;
	if (mandatory == KEY_MAIN_EXTDEPS) {
		idref = sv_before->cname.idext ;
		nref = sv_before->cname.next ;
		ext = 1 ;
	}
	// only pass here for the first time
	if (!tree_list->len)
	{
		if (!sastr_dir_get(tree_list, btmp,SS_BACKUP + 1, S_IFDIR)) log_warnusys_return(LOG_EXIT_ZERO,"get list of tree at: ",btmp) ;
	}

	if (nref)
	{
		// may have no tree yet
		if (tree_list->len)
		{
			size_t id = idref, nid = nref ;
			for (;nid; id += strlen(deps.s + id) + 1, nid--)
			{

				newsv.len = 0 ;
				optname = deps.s + id ;

				for(pos = 0 ; pos < tree_list->len ; pos += strlen(tree_list->s + pos) +1 )
				{
					found = 0 ;
					char *tree = tree_list->s + pos ;
					if (obstr_equal(tree,info->treename.s)) continue ; 
					size_t treelen = strlen(tree) ;
					char tmp[baselen + 1 + treelen + SS_SVDIRS_LEN + 1] ;
					auto_strings(tmp,btmp,"/",tree,SS_SVDIRS) ;
					
					parse_service_check_enabled(tmp,optname,force,&found) ;
					if (found)
					{
						log_trace(ext ? "external" : "optional"," service dependency: ",optname," is already enabled at tree: ",btmp,"/",tree) ;
						break ;
					}
				}
				if (!found)
				{
					// -1 mean system error. If the service doesn't exist it return 0
					r = ss_resolve_src_path(&newsv,optname,info->owner) ;
					if (r == -1) {
						goto err ; //don't warn here, the ss_revolve_src_path already warn user
					}
					else if (!r) {
						if (ext) goto err ;
					}
					// be paranoid with the else if
					else if (r == 1) {
						if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force))
							goto err ;
						// we only keep the first found on optsdepends
						if (!ext) break ;
					}
				}
			}
		}
	}

	stralloc_free(&newsv) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		return 0 ;
}

/** General helper */

int parse_service_check_enabled(char const *tree,char const *svname,uint8_t force,uint8_t *exist)
{
	/** char const tree -> tree.s + SS_SVDIRS */
	size_t namelen = strlen(svname), newlen = strlen(tree) ;
	char tmp[newlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen + 1] ;
	auto_strings(tmp,tree,SS_DB,SS_SRC,"/",svname) ;
	/** search in db first, the most used */
	if (scan_mode(tmp,S_IFDIR) > 0)
	{
		(*exist) = 1 ;
		if (!force) goto found ;
	}
	/** svc */
	auto_string_from(tmp,newlen,SS_SVC,"/",svname) ;
	if (scan_mode(tmp,S_IFDIR) > 0)
	{
		(*exist) = 1 ;
		if (!force) goto found ;
	}

	return 1 ;
	found:
		log_info("Ignoring: ",svname," service: already enabled") ;
		return 0 ;
}


int parse_add_service(stralloc *parsed_list,sv_alltype *sv_before,char const *service,unsigned int *nbsv,uid_t owner)
{
	stralloc conf = STRALLOC_ZERO ;
	size_t svlen = strlen(service) ;
	// keep source of the frontend file
	sv_before->src = keep.len ;
	if (!stralloc_catb(&keep,service,svlen + 1)) goto err ;
	// keep source of the configuration file
	if (sv_before->opts[2])
	{
		if (!env_resolve_conf(&conf,owner)) goto err ;
		sv_before->srconf = keep.len ;
		if (!stralloc_catb(&keep,conf.s,conf.len + 1)) goto err ;
	}
	// keep service on current list
	if (!stralloc_catb(parsed_list,service,svlen + 1)) goto err ;
	if (!genalloc_append(sv_alltype,&gasv,sv_before)) goto err ;
	(*nbsv)++ ;
	stralloc_free(&conf) ;
	return 1 ;
	err:
		stralloc_free(&conf) ;
		return 0 ;
}

/* module helper */

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
	if (!sastr_dir_get_recursive(&list,sdir,".configure",mode)) 
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
