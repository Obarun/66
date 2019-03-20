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
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/strakeyval.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/bytes.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/avltree.h>
#include <skalibs/diuint32.h>

#include <66/enum.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>

#include <stdio.h>
stralloc keep = STRALLOC_ZERO ;//sv_alltype data
stralloc deps = STRALLOC_ZERO ;//sv_name depends
stralloc saenv = STRALLOC_ZERO ;//sv_alltype env

genalloc ganame = GENALLOC_ZERO ;//sv_name_t, avltree
genalloc gadeps = GENALLOC_ZERO ;//unsigned int, pos in deps

genalloc gasv = GENALLOC_ZERO ;//sv_alltype general

void freed_parser(void)
{
	stralloc_free(&keep) ;
	stralloc_free(&deps) ;
	stralloc_free(&saenv) ;
	genalloc_free(sv_name_t,&ganame) ;
	genalloc_free(unsigned int,&gadeps) ;
	for (unsigned int i = 0 ; i < genalloc_len(sv_alltype,&gasv) ; i++)
		sv_alltype_free(&genalloc_s(sv_alltype,&gasv)[i]) ;
	avltree_free(&deps_map) ;
}

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

static int read_svfile(stralloc *sasv,char const *name,char const *src)
{
	int r ; 
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char svtmp[srclen + 1 + namelen + 1] ;
	memcpy(svtmp,src,srclen) ;
	svtmp[srclen] = '/' ;
	memcpy(svtmp + srclen + 1, name, namelen) ;
	svtmp[srclen + 1 + namelen] = 0 ;
	
	VERBO3 strerr_warni4x("Read service file of : ",name," at: ",src) ;
	
	size_t filesize=file_get_size(svtmp) ;
	if (!filesize)
	{
		VERBO3 strerr_warnw2x(svtmp," is empty") ;
		return 0 ;
	}
	sasv->len = 0 ;
	
	r = openreadfileclose(svtmp,sasv,filesize) ;
	if(!r)
	{
		VERBO3 strerr_warnwu2sys("open ", svtmp) ;
		return 0 ;
	}
	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(sasv,"\n")) retstralloc(0,"parse_service_before") ;
	if (!stralloc_0(sasv)) retstralloc(0,"parse_service_before") ;
	
	return 1 ;
}
int insta_replace(stralloc *sa,char const *src,char const *cpy)
{
	
	int curr, count ;
	
	if (!src || !*src) return 0;
	
	size_t len = strlen(src) ;
	size_t clen= strlen(cpy) ;
			
	curr = count = 0 ;
	for(int i = 0; (size_t)i < len;i++)
		if (src[i] == '@')
			count++ ;
		
	size_t resultlen = len + (clen * count) ;
	char result[resultlen + 1 ] ;
	
	for(int i = 0; (size_t)i < len;i++)
	{
		if (src[i] == '@')
		{
			
			if (((size_t)i + 1) == len) break ;
			if (src[i + 1] == 'I')
			{
				memcpy(result + curr,cpy,clen) ;
				curr = curr + clen;
				i = i + 2 ;
			}
		}
		result[curr++] = src[i] ;	
			
	}
	result[curr] = 0 ;
	
	return stralloc_obreplace(sa,result) ;
}
/** instance -> 0, copy -> 1 */
int insta_splitname(stralloc *sa,char const *name,int len,int what)
{
	char const *copy ;
	size_t tlen = len + 1 ;
	
	char template[tlen + 1] ;
	memcpy(template,name,tlen) ;
	template[tlen] = 0 ;
	
	copy = name + tlen ;
	
	if (!what)
		return stralloc_obreplace(sa,template) ;
	else
		return stralloc_obreplace(sa,copy) ;
}
int insta_create(stralloc *sasv,stralloc *sv, char const *src, int len)
{
	char *fdtmp ;
	char const *copy ;
	size_t tlen = len + 1 ;
	
	stralloc sa = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;	
	
	char template[tlen + 1] ;
	memcpy(template,sv->s,tlen) ;
	template[tlen] = 0 ;
	
	copy = sv->s + tlen ;

	fdtmp = dir_create_tmp(&tmp,"/tmp",copy) ;
	if(!fdtmp)
	{
		VERBO3 strerr_warnwu1x("create instance tmp dir") ;
		return 0 ;
	}

	if (!file_readputsa(&sa,src,template))
		strerr_diefu4sys(111,"open: ",src,"/",template) ;
	
	if (!insta_replace(&sa,sa.s,copy))
		strerr_diefu2x(111,"replace instance character at: ",sa.s) ;
	/** remove the last \0 */
	sa.len-- ;
	
	if (!file_write_unsafe(tmp.s,copy,sa.s,sa.len))
		strerr_diefu4sys(111,"create instance service file: ",tmp.s,"/",copy) ;
	
	if (!read_svfile(sasv,copy,tmp.s)) return 0 ;
	
	if (rm_rf(tmp.s) < 0)
		VERBO3 strerr_warnwu2x("remove tmp directory: ",tmp.s) ;
	
	stralloc_free(&sa) ;
	stralloc_free(&tmp) ;
	
	return stralloc_obreplace(sv,copy) ;
}

int insta_check(char const *svname)
{
	int r ;
		
	r = get_len_until(svname,'@') ;
	if (r < 0) return -1 ;
	
	return r ;
}

static int start_parser(stralloc *sasv,char const *name,sv_alltype *sv_before)
{
	VERBO2 strerr_warni3x("Parsing ", name," service...") ;
	
	int r ;
	/**@Return -5 for invalid svtype 
	* @Return -4 for invalid section
	* @Return -3 invalid key in section
	* @Return -2 for invalid keystyle
	* @Return -1 mandatory key is missing
	* @Return 0 unable to transfer to service struct*/
	r = parser(sasv->s,sv_before) ;
	switch(r)
	{
		case -5: VERBO3 strerr_warnw3x("invalid type for: ",name," service") ; return 0 ;
		case -4: VERBO3 strerr_warnw3x("invalid section for: ",name," service") ; return 0 ;
		case -3: VERBO3 strerr_warnw3x("invalid key in section for: ",name," service") ; return 0 ;
		case -2: VERBO3 strerr_warnw3x("invalid key value for: ",name," service") ; return 0 ;
		case -1: VERBO3 strerr_warnw3x("mandatory key is missing for: ",name," service") ; return 0 ;
		case  0: VERBO3 strerr_warnwu3x("keep information of: ",name," service") ; return 0 ;
		default: break ;
	}
	
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
	
	end:
		genalloc_free(diuint32,&tmpsrc) ;
		stralloc_free(&sa) ;
		stralloc_free(&home) ;
	return err ;
}

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 service already added */
int resolve_srcdeps(sv_alltype *sv_before,char const *mainsv, char const *src, char const *tree,unsigned int *nbsv, stralloc *sasv,unsigned int force)
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
			
			if (obstr_equal(dname.s,mainsv))
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
				if (obstr_equal(dname.s,maininsta.s))
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
			
			if (!start_parser(sasv,dname.s,&sv_before_deps)) return 0 ;
			
			r = resolve_srcdeps(&sv_before_deps,dname.s,newsrc.s,tree,nbsv,sasv,force) ;
			
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
		
	if (!start_parser(sasv,newsv.s,&sv_before)) return 0 ;
	
	if (sv_before.cname.itype > CLASSIC)
	{
		r = resolve_srcdeps(&sv_before,sv,src,tree,nbsv,sasv,force) ;
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

int get_section_range(char const *s, int idsec, genalloc *gasection)
{
	int err,r,base,pos,newpos ;
	size_t begin,end ;
	base = pos = newpos = begin = 0  ;
	size_t slen = strlen(s) ;
	char satmp[slen + 1] ;
	stralloc stmp = STRALLOC_ZERO ;
	stralloc sectmp = STRALLOC_ZERO ;
	stralloc secbase = STRALLOC_ZERO ;
	genalloc wasted = GENALLOC_ZERO ;//stralist
	
	err = 1 ;
	base = 0 ;
	stmp.len = 0 ; 
	sectmp.len = 0 ; 
	secbase.len = 0 ; 
	
	/** force to use the entire format of section*/
	if (!stralloc_cats(&sectmp,"[")) retstralloc(0,"get_section_range") ;
	if (!stralloc_cats(&sectmp,get_keybyid(idsec))) retstralloc(0,"get_section_range") ;
	if (!stralloc_cats(&sectmp,"]")) retstralloc(0,"get_section_range") ;
	if (!stralloc_0(&sectmp)) retstralloc(0,"get_section_range") ;

	pos = get_key(s,sectmp.s,&stmp) ;

	if (!get_wasted_line(stmp.s)) goto freed ;

	if (pos >= 0){
		r = get_cleansection(stmp.s,&secbase) ;
		if(r < 0) { err = 0 ; goto freed ; }
		stmp.len = 0 ; 
		sectmp.len = 0 ;
		end = get_len_until(s+pos,'\n') ;
		end++;
		pos = pos + end ;
		base = pos ;
		newpos = end = 0 ;
		loop:
			newpos = get_key(s+pos,"[",&stmp) ;
			if (newpos >= 0){
				sectmp.len = 0 ;
				r = get_cleansection(stmp.s,&sectmp) ;
				if (r<0){
					stmp.len = 0 ;
					pos = pos + newpos ;
					end = get_len_until(s+pos,'\n') ;
					end++;
					pos = pos + end ;
					goto loop ;
				} 
				else newpos = pos + newpos - base ; 
			} 
			else newpos = strlen(s) - base ;//end of string
	}else{
		if (!idsec) { err = 0 ; goto freed ; }
		//only main section are mandatory
		// so, even if the section was not found
		// return 1 ;
		goto freed ;
	}
	int nl ; 
	nl = get_nbline_ga(s+base,newpos,&wasted) ;
	
	for (int i = 0;i < genalloc_len(stralist,&wasted); i++){
		if (!get_wasted_line(gaistr(&wasted,i))) nl--;
	}
	if (!nl) { err = 0 ; goto freed ; }
	/** leak here */
	memcpy(satmp,s+base,newpos) ;
	satmp[newpos] = 0 ;
	if (!strakv_new(gasection,secbase.s,secbase.len,satmp,newpos)){ err = 0 ; goto freed ; }

	freed:
	stralloc_free(&stmp) ;
	stralloc_free(&sectmp) ;
	stralloc_free(&secbase) ;
	genalloc_deepfree(stralist,&wasted,stra_free) ;
	
	return err ;
}

int get_key_range(char const *s, int idsec, genalloc *ganocheck)
{
	int err,r,rn,rk ;
	size_t slen = strlen(s) ;
	size_t tplen = 0 ;
	char satmp[slen + 1] ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc kp = STRALLOC_ZERO ;
	
	key_all_t const *list = total_list ;
	
	err = 1 ;
	
	for (int i = 0;i<total_list_el[idsec];i++)
	{	
		keynocheck nocheck = KEYNOCHECK_ZERO ;
		
		r = -1 ;
		rk =  0 ;
		rn = 0 ;
		kp.len = 0 ;
		tmp.len = 0 ;
		
		if (idsec == ENV){
			if (list[idsec].list[i].name){
				r = get_keyline(s,"=",&tmp) ;
			}
			if (r < 0) continue ;
		}
		else {
			
			if (list[idsec].list[i].name){
				r = get_key(s,list[idsec].list[i].name,&tmp) ;
			}
			
			if (r < 0) continue ;
			
			if (!get_wasted_line(tmp.s))
				 continue ;
				
			rk = get_cleankey(tmp.s) ;
			if (!rk) { err = 0 ; goto freed ; }
			
			if (!obstr_equal(tmp.s,list[idsec].list[i].name))
				continue ;
			
			rk = get_len_until(s+r,'\n') ;
			if (rk < 0) { err = 0 ; goto freed ; }
			//rk++;//remove the last '\n' character
		}
		tplen = slen - (r+rk) ;
		memcpy(satmp,s+r+rk,tplen) ;
		satmp[tplen] = 0 ;
		if (!stralloc_cats(&kp,satmp)) retstralloc(0,"get_key_range") ;
		if (!stralloc_0(&kp)) retstralloc(0,"get_key_range") ;
		
		rn = get_nextkey(kp.s, idsec, list[idsec].list) ;
		
		nocheck.nkey = (genalloc_len(keynocheck,ganocheck))+1 ;
		nocheck.idsec = idsec ;
		/**special case for the environment section*/
		if (idsec == ENV) nocheck.idkey = ENVAL ;
		else nocheck.idkey = get_enumbyid(tmp.s,key_enum_el) ;
		
		nocheck.expected = list[idsec].list[i].expected ;
		nocheck.mandatory = list[idsec].list[i].mandatory ;
		if (rn < 0 || idsec == ENV){
			tplen = slen - r ;
			memcpy(satmp,s+r,tplen) ;
			satmp[tplen] = 0 ;
		}
		else
		{
			tplen = rk + rn ;
			memcpy(satmp,s+r,tplen) ;
			satmp[tplen] = 0 ;
		}
		
		if (!stralloc_catb(&nocheck.val,satmp,tplen)) retstralloc(0,"get_key_range") ;
		if (!stralloc_0(&nocheck.val)) retstralloc(0,"get_key_range") ;
		if (!genalloc_append(keynocheck,ganocheck,&nocheck)) retstralloc(0,"get_key_range") ;
		
	}
	freed:
	stralloc_free(&tmp) ;
	stralloc_free(&kp) ;
	if (!genalloc_len(keynocheck,ganocheck)) return 0 ;
	
	return err ;
}

int get_keystyle(keynocheck *nocheck)
{
	/** Remove key name on nocheck->val*/
	if (nocheck->idsec != ENV){
		char tmp[nocheck->val.len + 1] ;
		memcpy(tmp,nocheck->val.s,nocheck->val.len) ;
		tmp[nocheck->val.len] = 0 ;
		get_cleanval(tmp) ;
		if (!stralloc_obreplace(&nocheck->val,tmp)) return 0 ;
	}

	switch(nocheck->expected){
			
			case LINE:
				if (!parse_line(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse line for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			case QUOTE:
				if (!parse_quote(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse quote for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			case BRACKET:
			
				if (!parse_bracket(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse bracket for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			case UINT:
				if (!parse_line(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse uint for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			case SLASH:
				if (!parse_line(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse slash for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			case KEYVAL:
				if (!parse_env(nocheck))
				{
					VERBO3 strerr_warnwu2x("parse keyval for key: ",get_keybyid(nocheck->idkey)) ;
					return 0 ;
				}
				break ;
			default:
				VERBO3 strerr_warnw2x("unknown format : ",get_keybyid(nocheck->expected)) ;
				return 0 ;
		}

	return 1 ;
}

int get_mandatory(genalloc *nocheck,int idsec,int idkey)
{
	int count, bkey, r, countidsec ;
	
	key_all_t const *list = total_list ;
	
	genalloc gatmp = GENALLOC_ZERO ;
	r = 0 ;
	count = 0 ;
	bkey = -1 ;
	countidsec = 0 ;
			
	switch(list[idsec].list[idkey].mandatory){
		
		case NEED:
			for (unsigned int j = 0;j < genalloc_len(keynocheck,nocheck);j++)
			{
				if (genalloc_s(keynocheck,nocheck)[j].idsec == idsec)
				{
					countidsec++ ;
					if (genalloc_s(keynocheck,nocheck)[j].idkey == get_enumbyid(list[idsec].list[idkey].name,key_enum_el))
					{
						count++ ;
						break ;
					}
				}
			}					
			if ((!count) && (countidsec))
			{
				VERBO3 strerr_warnw4x("mandatory key: ",list[idsec].list[idkey].name," : not found on section: ",get_keybyid(idsec)) ;
				return 0 ;
			}
			break ;
		case CUSTOM:
			for (unsigned int j = 0;j < genalloc_len(keynocheck,nocheck);j++)
			{
				if (genalloc_s(keynocheck,nocheck)[j].idsec == idsec)
				{
					countidsec++ ;
					if (genalloc_s(keynocheck,nocheck)[j].idkey == BUILD)
					{
						bkey = j ;
						
					}
					if (genalloc_s(keynocheck,nocheck)[j].idkey == get_enumbyid(list[idsec].list[idkey].name,key_enum_el))
					{
						count++ ;
						break ;
					}
				}
			}	
			if((obstr_equal(genalloc_s(keynocheck,nocheck)[bkey].val.s,get_keybyid(CUSTOM))) && (!count) && (countidsec))
			{
				VERBO3 strerr_warnw5x("custom build asked on section: ",get_keybyid(idsec),", key: ",list[idsec].list[idkey].name," must be set") ;
				return 0 ;
			}
			break ;
		case BUNDLE:
			for (unsigned int j = 0;j < genalloc_len(keynocheck,nocheck);j++)
			{
				if (genalloc_s(keynocheck,nocheck)[j].idsec == idsec)
				{
					countidsec++ ;
					if (genalloc_s(keynocheck,nocheck)[j].idkey == TYPE)
					{
						bkey = j;
						
					}
					if (genalloc_s(keynocheck,nocheck)[j].idkey == get_enumbyid(list[idsec].list[idkey].name,key_enum_el))
					{
						count++ ;
					}
				}
			}
			if ((obstr_equal(genalloc_s(keynocheck,nocheck)[bkey].val.s,get_keybyid(BUNDLE))) && (!count) && (countidsec))
			{
				VERBO3 strerr_warnw1x("bundle type: key @contents must be set") ;
				return 0 ;
			}
			break ;
		/** only pass through here to check if flags env was asked
		 * and the corresponding section exist*/	
		case OPTS:
			for (unsigned int j = 0;j < genalloc_len(keynocheck,nocheck);j++)
			{
				if (genalloc_s(keynocheck,nocheck)[j].idsec == ENV)
					count++ ;
				
				if (genalloc_s(keynocheck,nocheck)[j].idsec == START||STOP) 
				{
					if (genalloc_s(keynocheck,nocheck)[j].idkey == OPTIONS)
					{
						bkey = j;						
					}
				}
			}
			if (bkey >= 0)
			{
				if (!clean_val(&gatmp,genalloc_s(keynocheck,nocheck)[bkey].val.s)) strerr_diefu2x(111,"parse file ",genalloc_s(keynocheck,nocheck)[bkey].val.s) ;
				r = stra_cmp(&gatmp,get_keybyid(ENVIR)) ;
				if ((r) && (!count))
				{
					VERBO3 strerr_warnw1x("flags env was asked, section environment must be set") ;
					return 0 ;
				}
			}
			
			break ;
		default:
			/*VERBO3
			{
				strerr_warnw2x("unknown mandatory type: ",get_keybyid(list[idsec].list[idkey].mandatory)) ; 
				//return 0 ;
			}*/
			break ;
	}
			

	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
}

int nocheck_toservice(keynocheck *nocheck,int svtype, sv_alltype *service)
{
	static unsigned char const actions[5][4] = {
	 //c->CLASSIC,		BUNDLE,	LONGRUN,	ONESHOT 
	    { COMMON,		COMMON,	COMMON,		COMMON }, // main
	    { EXECRUN, 		SKIP,	EXECRUN, 	EXECUP }, // start
	    { EXECFINISH, 	SKIP,	EXECFINISH, EXECDOWN }, // stop
	    { EXECLOG,		SKIP,	EXECLOG, 	SKIP }, // log
	    { ENVIRON, 		SKIP,	ENVIRON, 	ENVIRON }, // env
	} ;
	static unsigned char const states[5][4] = {
	 //c->CLASSIC,	BUNDLE,	LONGRUN,	ONESHOT
	    { START,	SKIP,	START,	START }, // main
	    { STOP,		SKIP,	STOP,	STOP }, // start
	    { LOG,		SKIP,	LOG,	LOG }, // stop
	    { ENV,		SKIP,	ENV,	ENV }, // log
	    { SKIP,		SKIP,	SKIP,  	SKIP }, // env
	     
	} ;
	int  p ;
	p = svtype ;
	int state = 0 ;
	
	while (state < 6)
	{
	    unsigned int c = p - CLASSIC; 
	    unsigned int action = actions[state][c] ;
	    state = states[state][c] ;
	
	    switch (action) {
			case COMMON:
				if (!nocheck->idsec)
					if (!keep_common(service,nocheck,svtype))
					{
						VERBO3 strerr_warnwu1x("keep common") ;
						return 0 ;
					} 
				break ;
			case EXECRUN:
				if (nocheck->idsec == 1)
					if (!keep_runfinish(&service->type.classic_longrun.run,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep execrun") ;
						return 0 ;
					} 
				break ;
			case EXECFINISH:
				if (nocheck->idsec == 2)
					if (!keep_runfinish(&service->type.classic_longrun.finish,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep execfinish") ;
						return 0 ;
					} 
				break ;
			case EXECLOG:
				if (nocheck->idsec == 3)
					if (!keep_logger(&service->type.classic_longrun.log,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep execlog") ;
						return 0 ;
					} 
				break ;
			case EXECUP:
				if (nocheck->idsec == 1)
					if (!keep_runfinish(&service->type.oneshot.up,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep execup") ;
						return 0 ;
					} 
				break ;
			case EXECDOWN:
				if (nocheck->idsec == 2)
					if (!keep_runfinish(&service->type.oneshot.down,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep execdown") ;
						return 0 ;
					} 
				break ;
			case ENVIRON:
				if (nocheck->idsec == 4)
					if (!keep_common(service,nocheck,svtype))
					{
						VERBO3 strerr_warnwu1x("keep environ") ;
						return 0 ;
					} 
				break ;
			case SKIP:
				break ;
			default:
				VERBO3 strerr_warnw1x("unknown action") ;
				return 0 ;
			}
	}
	
	return 1 ;
}
/**@Return -5 for invalid svtype 
 * @Return -4 for invalid section
 * @Return -3 invalid key in section
 * @Return -2 for invalid keystyle
 * @Return -1 mandatory file is missing
 * @Return 0 unable to transfer to service struct*/
int parser(char const *str,sv_alltype *service)
{
	int err, idsec, svtype ;
	
	genalloc section = GENALLOC_ZERO ;//stra_keyval type
	genalloc nocheck = GENALLOC_ZERO ;//keynocheck type
	
	err = 1 ;
		
	for (int i = 0 ; i<key_enum_section_el;i++)
		if (!get_section_range(str,i,&section)){ err = -4 ; goto freed ; }
			
	for (unsigned int i = 0;i < genalloc_len(strakeyval,&section);i++)
	{
		idsec = get_enumbyid(gaikvkey(&section,i),key_enum_section_el) ;
		if (!get_key_range(gaikvval(&section,i),idsec,&nocheck)) { err = -3 ; goto freed ; }
	}
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		if (!get_keystyle(&(genalloc_s(keynocheck,&nocheck)[i]))) { err = -2 ; goto freed ; }
	}
	svtype = get_enumbyid(genalloc_s(keynocheck,&nocheck)[0].val.s,key_enum_el) ;
	if (svtype < 0) { err = -5 ; goto freed ; }
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		idsec = genalloc_s(keynocheck,&nocheck)[i].idsec ;
		
		for (int j = 0;j < total_list_el[idsec] && total_list[idsec].list > 0;j++)
			if (!get_mandatory(&nocheck,idsec,j)) { err = -1 ; goto freed ; }
	}
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		if (!nocheck_toservice(&(genalloc_s(keynocheck,&nocheck)[i]),svtype,service)) { err = 0 ; goto freed ; }
	}
	if ((service->opts[1]) && (svtype == LONGRUN))
	{
		if (!add_pipe(service, &deps))
		{
			VERBO3 strerr_warnwu2x("add pipe: ", keep.s+service->cname.name) ;
			{ err = 0 ; goto freed ; };
		} 
	}
	
	freed:
		genalloc_deepfree(strakeyval,&section,strakv_free) ;
		genalloc_deepfree(keynocheck,&nocheck,keynocheck_free) ;

	return err ;
}

			
