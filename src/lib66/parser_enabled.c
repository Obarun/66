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
#include <oblibs/stralist.h>
#include <oblibs/directory.h>
#include <oblibs/error2.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/avltree.h>
#include <skalibs/diuint32.h>

#include <66/enum.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/resolve.h>

static void *sv_toadd (unsigned int d, void *x)
{
	return (void *)(keep.s + genalloc_s(sv_name_t, (genalloc *)x)[d].name) ;
	
}

static int sv_cmp(void const *a, void const *b, void *x)
{
	(void)x ;
	return strcmp((char const *)a, (char const *)b) ;
}

avltree deps_map = AVLTREE_INIT(8, 3, 8, &sv_toadd, &sv_cmp, &ganame) ;

int add_cname(genalloc *ga,avltree *tree,char const *name, sv_alltype *sv_before)
{
	uint32_t id = genalloc_len(sv_name_t,ga) ;
	
	if (!genalloc_append(sv_name_t,ga,&sv_before->cname)) retstralloc(0,"add_name") ;
	
	return avltree_insert(tree,id) ;
}

static int add_sv(sv_alltype *sv_before,char const *name,unsigned int *nbsv)
{
	int r ;
	
	if (!genalloc_append(sv_alltype,&gasv,sv_before)) retstralloc(0,"add_sv") ;
	
	r = add_cname(&ganame,&deps_map,name,sv_before) ;
	if (!r)
	{
		VERBO3 strerr_warnwu3x("insert", name," as node") ;
		return 0 ;
	}
	(*nbsv)++ ;
	
	VERBO2 strerr_warni2x("Service parsed successfully: ",name) ;
	
	return 1 ;
}

static int deps_src(stralloc *newsrc, char const *name, char const *tree, unsigned int force)
{
	int r, err ;
	uint32_t avlid ;
	unsigned int found = 0 ;
	uid_t owner = MYUID ;
	
	VERBO3 strerr_warni3x("Resolving source of ", name, " service dependency") ;
	r = avltree_search(&deps_map,name,&avlid) ;
	if (r) return 2 ; //already added nothing to do
	
	genalloc tmpsrc = GENALLOC_ZERO ; //type diuint32
	stralloc sa = STRALLOC_ZERO ;
	stralloc home = STRALLOC_ZERO ;
	
	newsrc->len = 0 ;
	err = 1 ;
	
	if (!set_ownerhome(&home,owner))
	{
		VERBO3 strerr_warnwu1sys("set home directory") ; 
		err = 0 ; goto end ;
	}
	
	if (!stralloc_cats(newsrc,tree)) retstralloc(0,"deps_src") ;
	if (!stralloc_cats(newsrc,SS_SVDIRS)) retstralloc(0,"deps_src") ;
	if (!stralloc_cats(newsrc,SS_DB)) retstralloc(0,"deps_src") ;
	if (!stralloc_cats(newsrc,SS_SRC)) retstralloc(0,"deps_src") ;
	if (!stralloc_0(newsrc)) retstralloc(0,"deps_src") ;
	r = dir_search(newsrc->s,name,S_IFDIR) ;
	if (r && !force) { err=2 ; goto end ; }
	else if (r < 0)
	{
		VERBO3 strerr_warnw3x("Conflicting format type for ",name," service file") ;
		return 0 ;
	}
	
	if (!owner)
	{
		if (!stralloc_obreplace(newsrc, SS_SERVICE_SYSDIR)) retstralloc(0,"deps_src") ;
	}
	else
	{
		if (!stralloc_cats(&home,SS_SERVICE_USERDIR)) retstralloc(0,"deps_src") ;
		if (!stralloc_0(&home)) retstralloc(0,"deps_src") ;
		if (!stralloc_obreplace(newsrc, home.s)) retstralloc(0,"deps_src") ;
	}
	
	r = ss_resolve_src(&tmpsrc,&sa,name,newsrc->s,&found) ; 
	if (r < 0) strerr_diefu2sys(111,"parse source directory: ",newsrc->s) ;
	if (!r)
	{
		if (!stralloc_obreplace(newsrc, SS_SERVICE_SYSDIR)) retstralloc(0,"deps_src") ;
		r = ss_resolve_src(&tmpsrc,&sa,name,newsrc->s,&found) ;
		if (r < 0) strerr_diefu2sys(111,"parse source directory: ",newsrc->s) ;
		if (!r)
		{
			if (!stralloc_obreplace(newsrc, SS_SERVICE_PACKDIR)) retstralloc(0,"deps_src") ;
			r = ss_resolve_src(&tmpsrc,&sa,name,newsrc->s,&found) ;
			if (r < 0) strerr_diefu2sys(111,"parse source directory: ",newsrc->s) ;
			if (!r)
			{
				VERBO3 strerr_warnwu2sys("find dependency ",name) ;
				err = 0 ;
				goto end ;
			}
		}
		if (!stralloc_obreplace(newsrc, sa.s + genalloc_s(diuint32,&tmpsrc)->right)) retstralloc(0,"deps_src") ;
	}
	else if (!stralloc_obreplace(newsrc, sa.s + genalloc_s(diuint32,&tmpsrc)->right)) retstralloc(0,"deps_src") ;
	
	end:
		genalloc_free(diuint32,&tmpsrc) ;
		stralloc_free(&sa) ;
		stralloc_free(&home) ;
	return err ;
}

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 service already added */
int resolve_srcdeps(sv_alltype *sv_before,char const *gensv,char const *mainsv, char const *src, char const *tree,unsigned int *nbsv, stralloc *sasv,unsigned int force)
{
	int r, insta ;
	
	size_t srclen ;
	
	stralloc newsrc = STRALLOC_ZERO ;
	genalloc ga = GENALLOC_ZERO ;//stralist
	
	
	stralloc dname = STRALLOC_ZERO ;
	stralloc maininsta = STRALLOC_ZERO ;
	uint32_t avlid = 0 ;
	
		
	if (sv_before->cname.itype == CLASSIC)
	{
		VERBO3 strerr_warnw3x("invalid service type compatibility for ",mainsv," service") ;
		return 0 ;
	}
	r = avltree_search(&deps_map,mainsv,&avlid) ;
	if (r)
	{
		VERBO3 strerr_warni3x("ignore ",mainsv," service dependency: already added") ;
		return 2 ;
	}
	
	insta = insta_check(mainsv) ;
	if (insta > 0)
	{
		if (!insta_splitname(&maininsta,mainsv,insta,0))
		{
			VERBO3 strerr_warnwu2x("split source name of instance: ",mainsv) ;
			return 0 ;
		}
	}
	
	if (sv_before->cname.nga)
	{
		if (sv_before->cname.itype != BUNDLE)
		{
			VERBO3 strerr_warni3x("Resolving ",mainsv," service dependencies") ;
		}else VERBO3 strerr_warni2x("Resolving service declaration of bundle: ",mainsv) ;
		
		for (int i = 0;i < sv_before->cname.nga;i++)
		{
			if (sv_before->cname.itype != BUNDLE)
			{
				VERBO3 strerr_warni4x("Service : ",mainsv, " depends on : ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i])) ;
			}else VERBO3 strerr_warni5x("Bundle : ",mainsv, " contents : ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i])," as service") ;
		
			if(!stra_add(&ga,deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before->cname.idga+i]))) return 0 ;
		}
		
		for (int i = 0;i < genalloc_len(stralist,&ga);i++)
		{	
			sv_alltype sv_before_deps = SV_ALLTYPE_ZERO ;
			newsrc.len = 0 ;
			char *dname_src = gaistr(&ga,i) ;
			
			if (!stralloc_obreplace(&dname,dname_src)) retstralloc(0,"resolve_srcdeps") ;
			
			if (obstr_equal(dname.s,mainsv) || obstr_equal(dname.s,gensv))
			{
				VERBO1 strerr_warnw3x("direct cyclic dependency detected on ",mainsv," service") ;
				return 0 ;
			}
			insta = insta_check(dname.s) ;
			if (!insta) 
			{
				VERBO3 strerr_warnw2x("invalid instance name: ",dname.s) ;
				return 0 ;
			}
	
			if (insta > 0)
			{
				if (!insta_splitname(&dname,dname_src,insta,1))
				{
					VERBO3 strerr_warnwu2x("split copy name of instance: ",dname_src) ;
					return 0 ;
				}
			}
			
			r = avltree_search(&deps_map,dname.s,&avlid) ;
			if (r) continue ;
			if (insta > 0)
			{
				if (!insta_splitname(&dname,dname_src,insta,0))
				{
					VERBO3 strerr_warnwu2x("split source name of instance: ",dname_src) ;
					return 0 ;
				}
				if (maininsta.len && obstr_equal(dname.s,maininsta.s))
				{
					VERBO3 strerr_warnw3x("direct cyclic instance dependency detected on ",mainsv," service") ;
					return 0 ;
				}
			}
			r = deps_src(&newsrc,dname.s,tree,force) ;
			if (!r) return 0 ;
			else if (r == 2)
			{ 
				VERBO3 strerr_warni3x("ignore ",dname.s," service dependency: already added") ;
				continue ;
			}
			if (insta > 0)
			{
				if (!stralloc_obreplace(&dname,dname_src)) retstralloc(0,"resolve_srcdeps") ;
				if (!insta_create(sasv,&dname,newsrc.s,insta))
				{
					VERBO3 strerr_warnwu2x("create instance service: ",dname.s) ;
					return 0 ;
				}
			}else if (!read_svfile(sasv,dname.s,newsrc.s)) return 0 ;
			
			if (!parser(&sv_before_deps,sasv,dname.s)) return 0 ;
			
			r = resolve_srcdeps(&sv_before_deps,mainsv,dname.s,newsrc.s,tree,nbsv,sasv,force) ;
			
			if (!r) return 0 ;
			if (r == 2) continue ;
		}
	}
	else VERBO3 strerr_warni3x("service: ",mainsv," haven't dependencies") ;
	
	srclen = strlen(src) ;
	sv_before->src = keep.len ;
	if (!stralloc_catb(&keep,src,srclen + 1)) retstralloc(0,"resolve_srcdeps") ;
	
	r = avltree_search(&deps_map,mainsv,&avlid) ;
	if (!r)	if (!add_sv(sv_before,mainsv,nbsv)) return 0 ;
	
	stralloc_free(&newsrc) ;
	stralloc_free(&dname) ;
	stralloc_free(&maininsta) ;
	genalloc_deepfree(stralist,&ga,stra_free) ;

	return 1 ;
}

int parse_service_before(char const *src,char const *sv,char const *tree, unsigned int *nbsv, stralloc *sasv,unsigned int force)
{
	int r = 0 ;
	
	size_t srclen ; 
	uint32_t id ;
	
	stralloc newsv = STRALLOC_ZERO ;
		
	if (!stralloc_cats(&newsv,sv)) retstralloc(0,"parse_service_before");
	if (!stralloc_0(&newsv)) retstralloc(0,"parse_service_before") ;
	
	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	r = insta_check(newsv.s) ;
	if (!r) 
	{
		VERBO3 strerr_warnw2x("invalid instance name: ",newsv.s) ;
		return 0 ;
	}
	if (r > 0)
	{
		
		if (!insta_create(sasv,&newsv,src,r))
		{
			VERBO3 strerr_warnwu2x("create instance service: ",newsv.s) ;
			return 0 ;
		}
	
	}else if (!read_svfile(sasv,newsv.s,src)) return 0 ;
	
		
	r = avltree_search(&deps_map,newsv.s,&id) ;
	if (r)
	{
		VERBO2 strerr_warni3x("ignore ",newsv.s," service: already added") ;
		sasv->len = 0 ;
		stralloc_free(&newsv) ;
		sv_alltype_free(&sv_before) ;
		return 1 ;
	}
		
	if (!parser(&sv_before,sasv,newsv.s)) return 0 ;
	
	if (sv_before.cname.itype > CLASSIC)
	{
		r = resolve_srcdeps(&sv_before,sv,sv,src,tree,nbsv,sasv,force) ;
		if (!r) return 0 ;
	}
	else
	{
		srclen = strlen(src) ;
		sv_before.src = keep.len ;
		if (!stralloc_catb(&keep,src,srclen + 1)) retstralloc(0,"parse_service_before") ;
		if (!add_sv(&sv_before,newsv.s,nbsv)) return 0 ;		
	}
	stralloc_free(&newsv) ;
	
	return 1 ;
}
