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
#include <oblibs/directory.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

int parse_service_before(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t conf)
{
	log_trace("start parse process of service: ",sv) ;
	int insta ;
	uint8_t exist = 0 ;
	size_t svlen = strlen(sv), svsrclen, svnamelen ;
	stralloc svclassic = STRALLOC_ZERO ;
	char svname[svlen + 1], svsrc[svlen + 1] ; 
	if (!ob_basename(svname,sv)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",sv) ;
	if (!ob_dirname(svsrc,sv)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",sv) ;
	svsrclen = strlen(svsrc) ;
	svnamelen = strlen(svname) ;
	char svpath[svsrclen + svnamelen + 1] ;
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
	
	/** contain of directory should be listed by ss_resolve_src_path
	 * execpt for module type */
	if (scan_mode(sv,S_IFDIR) == 1 && sv_before.cname.itype != TYPE_MODULE) return 1 ;
	
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
		svclassic.len = 0 ;
		r = parse_module(&svclassic,&sv_before,svname,info->owner,force,conf) ;
		if (!r) return 0 ;
		else if (r == 2)
		{
			sasv->len = 0 ;
			sv_alltype_free(&sv_before) ;
			goto deps ;
		}
		if (force > 1 && exist)
		{
			char const *newargv[4] ;
			unsigned int m = 0 ;
			
			newargv[m++] = "fake_name" ;
			newargv[m++] = svname ;
			newargv[m++] = 0 ;
			if (ssexec_disable(m,newargv,(const char *const *)environ,info)) 
				log_warnu_return(LOG_EXIT_ZERO,"disable module: ",svname) ;	
		}
		deps:
		if (svclassic.len)
		{
			size_t pos = 0 ;
			for (; pos < svclassic.len ; pos += strlen(svclassic.s + pos) + 1)
			{
				char *sv = svclassic.s + pos ;
				if (!parse_service_before(info,parsed_list,tree_list,sv,nbsv,sasv,force,conf)) return 0 ;
			}
		}
	}

	if (!parse_add_service(parsed_list,&sv_before,svpath,nbsv,info->owner)) return 0 ;
	
	if ((sv_before.cname.itype > TYPE_CLASSIC && force > 1) || !exist)
	{
		if (!parse_service_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,conf)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,conf,KEY_MAIN_EXTDEPS)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,conf,KEY_MAIN_OPTSDEPS)) return 0 ;
	}
	freed:
		stralloc_free(&svclassic) ;
	return 1 ;
}

int parse_service_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force,uint8_t conf)
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

			if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,conf)) goto err ;
		}
	}
	else log_trace(sv,": haven't dependencies") ;
	stralloc_free(&newsv) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		return 0 ;
}

int parse_service_opts_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list,sv_alltype *sv_before,char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force,uint8_t conf, uint8_t mandatory)
{
	int r ;
	stralloc newsv = STRALLOC_ZERO ;
		
	size_t pos = 0 , baselen = strlen(info->base.s) + SS_SYSTEM_LEN ;
	uint8_t found = 0, ext = mandatory == KEY_MAIN_EXTDEPS ? 1 : 0 ;
	char *optname = 0 ;
	char btmp[baselen + 1] ;
	auto_strings(btmp,info->base.s,SS_SYSTEM) ;
	
	int idref = sv_before->cname.idopts ;
	unsigned int nref = sv_before->cname.nopts ;
	if (ext) {
		idref = sv_before->cname.idext ;
		nref = sv_before->cname.next ;
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
						if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,conf))
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
