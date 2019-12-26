/* 
 * parser.c
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

#include <66/parser.h>

#include <string.h>
//#include <stdio.h>

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

int parse_service_check_enabled(ssexec_t *info, char const *svname,uint8_t force,uint8_t *exist)
{
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	int ret = 1 ;
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		log_warnusys("set revolve pointer to source") ;
		goto err ;	
	} 
	if (ss_resolve_check(sares.s,svname))
	{
		if (!ss_resolve_read(&res,sares.s,svname)) 
		{
			log_warnusys("read resolve file of: ",svname) ;
			goto err ;
		}
		if (res.disen)
		{
			(*exist) = 1 ;
			if (!force) { 
				log_warn("Ignoring: ",svname," service: already enabled") ;
				ret = 2 ;
				goto freed ;
			}
		}
	}
	freed:
	stralloc_free(&sares) ;
	ss_resolve_free(&res) ;
	return ret ;
	err:
		stralloc_free(&sares) ;
		ss_resolve_free(&res) ;
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

int parse_service_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force)
{
	int r ;
	uint8_t exist = 0 ;
	char *dname = 0 ;
	stralloc newsv = STRALLOC_ZERO ;
	if (sv_before->cname.nga)
	{
		size_t id = sv_before->cname.idga, nid = sv_before->cname.nga ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			newsv.len = 0 ;
			if (sv_before->cname.itype != BUNDLE)
			{
				log_trace("Service : ",sv, " depends on : ",deps.s+id) ;
			}else log_trace("Bundle : ",sv, " contents : ",deps.s+id," as service") ;
			dname = deps.s + id ;
			r = ss_resolve_src_path(&newsv,dname,info) ;
			if (r < 1) goto err ;//don't warn here, the ss_revolve_src_path already warn user

			if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,&exist)) goto err ;
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
	ss_resolve_t res = RESOLVE_ZERO ;
	
	size_t pos = 0 , baselen = strlen(info->base.s) + SS_SYSTEM_LEN ;
	uint8_t exist = 0, found = 0, ext = 0 ;
	char *optname = 0 ;
	char btmp[baselen + 1] ;
	auto_strings(btmp,info->base.s,SS_SYSTEM) ;
	
	unsigned int idref = sv_before->cname.idopts ;
	unsigned int nref = sv_before->cname.nopts ;
	if (mandatory == EXTDEPS) {
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

					// already added on a tree
					if (ss_resolve_check(tmp,optname))
					{
						if (!ss_resolve_read(&res,tmp,optname)) log_warnusys_return(LOG_EXIT_ZERO,"read resolve file of: ",optname) ;
						if (res.disen)
						{
							found = 1 ;
							log_trace(ext ? "external" : "optional"," service dependency: ",optname," is already enabled at tree: ",btmp,"/",tree) ;
							break ;
						}
					}
				}
				if (!found)
				{
					// -1 mean system error. If the service doesn't exist it return 0
					r = ss_resolve_src_path(&newsv,optname,info) ;
					if (r == -1) {
						goto err ; //don't warn here, the ss_revolve_src_path already warn user
					}
					else if (!r) {
						if (ext) goto err ;
					}
					// be paranoid with the else if
					else if (r == 1) {
						if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,&exist))
							goto err ;
						// we only keep the first found on optsdepends
						if (!ext) break ;
					}
				}
			}
		}
	}

	stralloc_free(&newsv) ;
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		return 0 ;
}

int parse_service_before(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t *exist)
{
	
	int r, insta ;
	size_t svlen = strlen(sv), svsrclen, svnamelen ;
	char svname[svlen + 1], svsrc[svlen + 1] ; 
	if (!basename(svname,sv)) return 0 ;
	if (!dirname(svsrc,sv)) return 0 ;
	svsrclen = strlen(svsrc) ;
	svnamelen = strlen(svname) ;
	char svpath[svsrclen + svnamelen + 1] ;
	if (scan_mode(sv,S_IFDIR) == 1) return 1 ;
		
	r = parse_service_check_enabled(info,svname,force,exist) ;
	if (r == 2) goto freed ;
	else if (!r) return 0 ;
	
	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	sasv->len = 0 ;

	insta = instance_check(svname) ;
	if (!insta) 
		log_warn_return(LOG_EXIT_ZERO, "invalid instance name: ",svname) ;

	if (insta > 0)
	{
		if (!instance_create(sasv,svname,SS_INSTANCE,svsrc,insta))
			log_warn_return(LOG_EXIT_ZERO,"create instance service: ",svname) ;
		
		/** ensure that we have an empty line at the end of the string*/
		if (!stralloc_cats(sasv,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(sasv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}else if (!read_svfile(sasv,svname,svsrc)) return 0 ;
	
	memcpy(svpath,svsrc,svsrclen) ;
	memcpy(svpath + svsrclen,svname,svnamelen) ;
	svpath[svsrclen + svnamelen] = 0 ;
	
	if (sastr_cmp(parsed_list,svpath) >= 0)
	{
		log_trace(sv,": already added") ;
		sasv->len = 0 ;
		sv_alltype_free(&sv_before) ;
		goto freed ;
	}

	if (!parser(&sv_before,sasv,svname)) return 0 ;
	
	/** keep the name set by user
	 * uniquely for instantiated service
	 * The name must contain the template string */
	
	if (insta > 0 && sv_before.cname.name >= 0 )
	{
		stralloc sainsta = STRALLOC_ZERO ;
		stralloc name = STRALLOC_ZERO ;
		if (!stralloc_cats(&name,keep.s + sv_before.cname.name)) goto err ;
		
		if (!instance_splitname(&sainsta,svname,insta,0)) goto err ;
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
	if (!parse_add_service(parsed_list,&sv_before,svpath,nbsv,info->owner)) return 0 ;
	
	if ((sv_before.cname.itype > CLASSIC && force > 1) || !(*exist))
	{
		if (!parse_service_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,OPTSDEPS)) return 0 ;
		if (!parse_service_opts_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,EXTDEPS)) return 0 ;
	}
	freed:
	return 1 ;
}
