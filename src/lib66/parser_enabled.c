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
#include <oblibs/error2.h>
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
		VERBO3 strerr_warnwu1sys("set revolve pointer to source") ;
		goto err ;	
	} 
	if (ss_resolve_check(sares.s,svname))
	{
		if (!ss_resolve_read(&res,sares.s,svname)) 
		{
			VERBO3 strerr_warnwu2sys("read resolve file of: ",svname) ;
			goto err ;
		}
		if (res.disen)
		{
			(*exist) = 1 ;
			if (!force) { 
				VERBO1 strerr_warnw3x("Ignoring: ",svname," service: already enabled") ;
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

int parse_service_deps(ssexec_t *info,stralloc *parsed_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force)
{
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
				VERBO3 strerr_warni4x("Service : ",sv, " depends on : ",deps.s+id) ;
			}else VERBO3 strerr_warni5x("Bundle : ",sv, " contents : ",deps.s+id," as service") ;
			dname = deps.s + id ;
			if (!ss_resolve_src_path(&newsv,dname,info))
			{
				VERBO3 strerr_warnwu2x("resolve source path of: ",dname) ;
				goto err ;
			}
			if (!parse_service_before(info,parsed_list,newsv.s,nbsv,sasv,force,&exist)) goto err ;
		}
	}
	else VERBO3 strerr_warni2x(sv,": haven't dependencies") ;
	stralloc_free(&newsv) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		return 0 ;
}

int parse_service_before(ssexec_t *info,stralloc *parsed_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t *exist)
{
	
	int r, insta ;
	size_t svlen = strlen(sv), svsrclen, svnamelen ;
	char svname[svlen + 1], svsrc[svlen + 1], svpath[svlen + 1] ;
	if (!basename(svname,sv)) return 0 ;
	if (!dirname(svsrc,sv)) return 0 ;
	svsrclen = strlen(svsrc) ;
	svnamelen = strlen(svname) ;
	if (scan_mode(sv,S_IFDIR) == 1) return 1 ;
		
	r = parse_service_check_enabled(info,svname,force,exist) ;
	if (r == 2) goto freed ;
	else if (!r) return 0 ;
	
	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	sasv->len = 0 ;

	insta = instance_check(svname) ;
	if (!insta) 
	{
		VERBO3 strerr_warnw2x("invalid instance name: ",svname) ;
		return 0 ;
	}
	if (insta > 0)
	{
		if (!instance_create(sasv,svname,SS_INSTANCE,svsrc,insta))
		{
			VERBO3 strerr_warnwu2x("create instance service: ",svname) ;
			return 0 ;
		}
		/** ensure that we have an empty line at the end of the string*/
		if (!stralloc_cats(sasv,"\n")) retstralloc(0,"parse_service_before") ;
		if (!stralloc_0(sasv)) retstralloc(0,"parse_service_before") ;
	}else if (!read_svfile(sasv,svname,svsrc)) return 0 ;
	
	memcpy(svpath,svsrc,svsrclen) ;
	memcpy(svpath + svsrclen,svname,svnamelen) ;
	svpath[svsrclen + svnamelen] = 0 ;
	
	if (sastr_cmp(parsed_list,svpath) >= 0)
	{
		VERBO2 strerr_warni2x(sv,": already added") ;
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
			strerr_warnw2x("invalid instantiated service name: ", keep.s + sv_before.cname.name) ;
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
		if (!parse_service_deps(info,parsed_list,&sv_before,sv,nbsv,sasv,force)) return 0 ;
	
	freed:
	return 1 ;
}
