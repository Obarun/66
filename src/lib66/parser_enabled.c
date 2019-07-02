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

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/error2.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>

int parse_service_get_list(stralloc *result, stralloc *list)
{
	unsigned int found ;
	size_t i = 0, len = list->len ;
	for (;i < len; i += strlen(list->s + i) + 1)
	{
		found = 0 ;
		char *name = list->s+i ;
		size_t svlen = strlen(name) ;
		char svname[svlen + 1] ;
		char svsrc[svlen + 1] ;
		if (!basename(svname,name)) return 0 ;
		if (!dirname(svsrc,name)) return 0 ;
		if (ss_resolve_src(result,svname,svsrc,&found) <= 0) return 0 ;
	}
	return 1 ;
}

int parse_add_service(stralloc *parsed_list,sv_alltype *sv_before,char const *service,unsigned int *nbsv)
{
	size_t svlen = strlen(service) ;
	char svsrc[svlen + 1] ;
	if (!dirname(svsrc,service)) return 0 ;
	size_t srclen = strlen(svsrc) ;
	sv_before->src = keep.len ;
	if (!stralloc_catb(&keep,svsrc,srclen + 1)) retstralloc(0,"parse_add_service") ;
	if (!stralloc_catb(parsed_list,service,svlen + 1)) retstralloc(0,"parse_add_service") ;
	if (!genalloc_append(sv_alltype,&gasv,sv_before)) retstralloc(0,"parse_add_service") ;
	(*nbsv)++ ;
	return 1 ;
	
}

int parse_service_deps(ssexec_t *info,stralloc *parsed_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,unsigned int force)
{
	char *dname = 0 ;
	stralloc newsv = STRALLOC_ZERO ;
	if (sv_before->cname.nga)
	{
		for (int i = 0;i < sv_before->cname.nga;i++)
		{
			newsv.len = 0 ;
			if (sv_before->cname.itype != BUNDLE)
			{
				VERBO3 strerr_warni4x("Service : ",sv, " depends on : ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i])) ;
			}else VERBO3 strerr_warni5x("Bundle : ",sv, " contents : ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i])," as service") ;
			
			dname = deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i]) ;
			if (!ss_resolve_src_path(&newsv,dname,info))
			{
				VERBO3 strerr_warnwu2x("resolve source path of: ",dname) ;
				stralloc_free(&newsv) ; 
				return 0 ;
			}
			if (!parse_service_before(info,parsed_list,newsv.s,nbsv,sasv,force))
			{ 
				stralloc_free(&newsv) ; 
				return 0 ;
			}
		}
	}
	else VERBO3 strerr_warni2x(sv,": haven't dependencies") ;
	stralloc_free(&newsv) ;
	return 1 ;
}

int parse_service_before(ssexec_t *info,stralloc *parsed_list, char const *sv,unsigned int *nbsv, stralloc *sasv,unsigned int force)
{
	
	int r = 0 , insta ;
	size_t svlen = strlen(sv), svsrclen ; 
	
	char svname[svlen + 1] ;
	char svsrc[svlen + 1] ;
	char svpath[svlen + 1] ;
	if (!basename(svname,sv)) return 0 ;
	if (!dirname(svsrc,sv)) return 0 ;
	svsrclen = strlen(svsrc) ;
	if (scan_mode(sv,S_IFDIR) == 1) return 1 ;
	
	stralloc newsv = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;

	{
		if (!set_ownerhome(&tmp,info->owner))
		{
			VERBO3 strerr_warnwu1sys("set home directory") ; 
			goto err ;
		}
		
		if (!stralloc_cats(&tmp,info->tree.s)) retstralloc(0,"parse_service_before") ;
		if (!stralloc_cats(&tmp,SS_SVDIRS)) retstralloc(0,"parse_service_before") ;
		if (!stralloc_cats(&tmp,SS_DB)) retstralloc(0,"parse_service_before") ;
		if (!stralloc_cats(&tmp,SS_SRC)) retstralloc(0,"parse_service_before") ;
		if (!stralloc_0(&tmp)) retstralloc(0,"parse_service_before") ;

		insta = insta_check(svname) ;
		if (!insta) 
		{
			VERBO3 strerr_warnw2x("invalid instance name: ",svname) ;
			goto err ;
		}
		if (insta > 0)
		{
			if (!insta_splitname(&newsv,svname,insta,1))
			{
				VERBO3 strerr_warnwu2x("split copy name of instance: ",svname) ;
				goto err ;
			}
		}
		else if (!stralloc_cats(&newsv,svname)) retstralloc(0,"parse_service_before") ;
		if (!stralloc_0(&newsv)) goto err ;
		r = dir_search(tmp.s,newsv.s,S_IFDIR) ;
		if (r && !force) { 
			VERBO2 strerr_warni2x(newsv.s,": already added") ;
			goto freed ;
		}
		else if (r < 0)
		{
			VERBO3 strerr_warnw3x("Conflicting format type for ",newsv.s," service file") ;
			goto err ;
		}
		newsv.len = 0 ;
	}
	if (!stralloc_cats(&newsv,svname)) goto err ;
	if (!stralloc_0(&newsv)) goto err ;
	
	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	insta = insta_check(newsv.s) ;
	if (!insta) 
	{
		VERBO3 strerr_warnw2x("invalid instance name: ",newsv.s) ;
		goto err ;
	}
	if (insta > 0)
	{
		
		if (!insta_create(sasv,&newsv,svsrc,insta))
		{
			VERBO3 strerr_warnwu2x("create instance service: ",newsv.s) ;
			goto err ;
		}
	
	}else if (!read_svfile(sasv,newsv.s,svsrc)) goto err ;
	
	memcpy(svpath,svsrc,svsrclen) ;
	memcpy(svpath + svsrclen,newsv.s,newsv.len) ;
	
	if (sastr_cmp(parsed_list,svpath) >= 0)
	{
		VERBO2 strerr_warni2x(sv,": already added") ;
		sasv->len = 0 ;
		sv_alltype_free(&sv_before) ;
		goto freed ;
	}
	
	if (!parser(&sv_before,sasv,newsv.s)) goto err ;
	
	if (!parse_add_service(parsed_list,&sv_before,svpath,nbsv)) goto err ;
	
	if (sv_before.cname.itype > CLASSIC)
		if (!parse_service_deps(info,parsed_list,&sv_before,sv,nbsv,sasv,force)) goto err ;
	
	freed:
		stralloc_free(&newsv) ;
		stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&newsv) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
