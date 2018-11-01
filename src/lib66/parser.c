/* 
 * parser.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
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

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/avltree.h>

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
	stralloc_free(&ganame) ;
	genalloc_free(unsigned int,&gadeps) ;
	genalloc_free(sv_alltype,&gasv) ;
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

int add_cname(char const *name, sv_alltype *sv_before,genalloc *ganame,avltree *deps_map)
{
	uint32_t id = genalloc_len(sv_name_t,ganame) ;
	
	if (!genalloc_append(sv_name_t,ganame,&sv_before->cname)) retstralloc(0,"add_name") ;
	
	return avltree_insert(deps_map,id) ;
}
/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 service already added */
int resolve_srcdeps(char const *src, char const *tree,avltree *deps_map,unsigned int *nbsv, unsigned int id,stralloc *sasv)
{
	int r ;

	stralloc sasrc = STRALLOC_ZERO ;
	char *name = NULL ;
	uint32_t avlid ;

	name = deps.s+(genalloc_s(unsigned int,&gadeps)[id]) ;
	
	VERBO2 strerr_warni3x("Resolving source of ", name, " service dependency") ;
	r = avltree_search(deps_map,name,&avlid) ;
	if (r)
	{
		stralloc_free(&sasrc) ;
		return 2 ;
	}
	/** Search in current dir*/
	if (!stralloc_cats(&sasrc,src)) retstralloc(0,"resolve_srcdeps") ;
	if (!stralloc_0(&sasrc)) retstralloc(0,"resolve_srcdeps") ;
	r = dir_search(sasrc.s,name,S_IFREG) ;
	if (!r)
	{
		/** Search on current tree*/
		if (!stralloc_obreplace(&sasrc, tree)) retstralloc(0,"resolve_deps") ;
		if (!stralloc_cats(&sasrc,tree)) retstralloc(0,"resolve_deps") ;
		if (!stralloc_cats(&sasrc,SS_SVDIRS)) retstralloc(0,"resolve_deps") ;
		if (!stralloc_cats(&sasrc,SS_DB)) retstralloc(0,"resolve_deps") ;
		if (!stralloc_cats(&sasrc,SS_SRC)) retstralloc(0,"resolve_deps") ;
		if (!stralloc_0(&sasrc)) retstralloc(0,"resolve_deps") ;
			
		r = dir_search(sasrc.s,name,S_IFDIR) ;
		
		if (r) return 2 ;// already on the tree, nothing to do here
		else 
		if (!r)
		{ 
			if (!stralloc_obreplace(&sasrc, SS_SERVICE_DIR)) retstralloc(0,"resolve_deps") ;
			r = dir_search(sasrc.s,name,S_IFREG) ;		
			if (r != 1)
			{
				VERBO3 strerr_warnwu2sys("find dependency ",name) ;
				return 0 ;
			}
		}
	}
	else 
	if (r < 0)
	{
		VERBO3 strerr_warnw3x("Conflicting format type for ",name," service file") ;
		return 0 ;
	}
	if (!parse_service_before(sasrc.s,name,tree,&deps,nbsv,sasv)) 
	{
		VERBO3 strerr_warnwu2x("parse service file: ",name) ;
		return 0 ;
	}
	
	return 1 ;
}

int parse_service_before(char const *src,char const *sv,char const *tree,stralloc *store, unsigned int *nbsv, stralloc *sasv)
{
	int r = 0 ;
	size_t srclen = strlen(src) ;
	size_t svlen = strlen(sv) ;
	uint32_t id ;

	sv_alltype sv_before = SV_ALLTYPE_ZERO ;
	stralloc srctmp = STRALLOC_ZERO ;
		
	char svtmp[srclen + svlen + 1] ;
	memcpy(svtmp,src,srclen) ;
	svtmp[srclen] = '/' ;
	memcpy(svtmp + srclen + 1, sv, svlen) ;
	svtmp[srclen + svlen + 1] = 0 ;
	
	size_t filesize=file_get_size(svtmp) ;
	if (!filesize)
	{
		VERBO3 strerr_warnw2x(svtmp," is empty") ;
		return 0 ;
	}
	r = openreadfileclose(svtmp,sasv,filesize) ;
	if(!r)
	{
		VERBO3 strerr_warnwu2sys("open ", svtmp) ;
		return 0 ;
	}
	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(sasv,"\n")) retstralloc(0,"parse_service_before") ;
	if (!stralloc_0(sasv)) retstralloc(0,"parse_service_before") ;
	
	VERBO2 strerr_warni3x("Parsing ", sv," service...") ;
	
	r = avltree_search(&deps_map,sv,&id) ;
	if (r)
	{
		VERBO3 strerr_warni3x("ignore ",sv," service: already added") ;
		sasv->len = 0 ;
		return 1 ;
	}
	/**@Return -5 for invalid svtype 
	* @Return -4 for invalid section
	* @Return -3 invalid key in section
	* @Return -2 for invalid keystyle
	* @Return -1 mandatory key is missing
	* @Return 0 unable to transfer to service struct*/
	r = parser(sasv->s,&sv_before) ;
	if (r == -5) VERBO3 strerr_warnw3x("invalid type for: ",sv," service") ;
	if (r == -4) VERBO3 strerr_warnw3x("invalid section for: ",sv," service") ;
	if (r == -3) VERBO3 strerr_warnw3x("invalid key in section for: ",sv," service") ;
	if (r == -2) VERBO3 strerr_warnw3x("invalid key value for: ",sv," service") ;
	if (r == -1) VERBO3 strerr_warnw3x("mandatory key is missing for: ",sv," service") ;
	if (!r) VERBO3 strerr_warnwu3x("keep information of: ",sv," service") ;
	
	if (r <= 0) return 0 ;
	
	sasv->len = 0 ;
	
	if (!genalloc_append(sv_alltype,&gasv,&sv_before)) retstralloc(0,"parse_service_before") ;
	
	r = add_cname(sv,&sv_before,&ganame,&deps_map) ;
	if (!r)
	{
		VERBO3 strerr_warnwu3x("insert", sv," as node") ;
		return 0 ;
	}
	(*nbsv)++ ;
		
	if (sv_before.cname.nga)
	{
		
		if (!stralloc_obreplace(&srctmp,src)) retstralloc(0,"parse_service_before") ;
		VERBO3 strerr_warni3x("Resolving ", keep.s+sv_before.cname.name, " service dependency") ;
		for (int i = 0;i < sv_before.cname.nga;i++)
		{
			if (sv_before.cname.itype == CLASSIC)
			{
				VERBO3 strerr_warnw3x("invalid service type compatibility for ",keep.s+sv_before.cname.name," service") ;
				return 0 ;
			}
			if (obstr_equal(deps.s+genalloc_s(unsigned int,&gadeps)[sv_before.cname.idga+i],keep.s+sv_before.cname.name))
			{
				VERBO3 strerr_warnw3x("direct cyclic dependency detected on ",keep.s+sv_before.cname.name," service") ;
				return 0 ;
			}
			r = avltree_search(&deps_map,deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before.cname.idga+i]),&id) ;
			if (r)
			{
				VERBO3 strerr_warni3x("ignore ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before.cname.idga+i])," service dependency: already added") ;
				continue ;
			}
			r = resolve_srcdeps(srctmp.s,tree,&deps_map,nbsv,sv_before.cname.idga+i,sasv) ;
			if (!r) return 0 ;
			if (r == 2)	VERBO3 strerr_warni3x("ignore ",deps.s+(genalloc_s(unsigned int,&gadeps)[sv_before.cname.idga+i])," service dependency: already added") ;
		}
	}

	stralloc_free(&srctmp) ;
	
	return 1 ;
}

int get_section_range(char const *s, int idsec, genalloc *gasection)
{
	int r,base,pos,newpos ;
	size_t begin,end ;
	base = pos = newpos = begin = 0  ;
	
	stralloc stmp = STRALLOC_ZERO ;
	stralloc sectmp = STRALLOC_ZERO ;
	stralloc secbase = STRALLOC_ZERO ;
	genalloc wasted = GENALLOC_ZERO ;
	
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

	if (pos >= 0){
		r = get_cleansection(stmp.s,&secbase) ;
		if(r<0) return 0 ;
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
		if (!idsec) return 0 ;
		//only main section are mandatory
		// so, even if the section was not found
		// return 1 ;
		return 1 ;
	}
	int nl ; 
	nl = get_nbline_ga(s+base,newpos,&wasted) ;
	
	for (int i = 0;i < genalloc_len(stralist,&wasted); i++){
		if (!get_wasted_line(gaistr(&wasted,i))) nl--;
	}
	if (!nl) return 0 ;
	
	if (!strakv_new(gasection,secbase.s,secbase.len,s+base,newpos)) return 0 ;

	stralloc_free(&stmp) ;
	stralloc_free(&sectmp) ;
	stralloc_free(&secbase) ;
	genalloc_deepfree(stralist,&wasted,stralloc_free) ;
	
	return 1 ;
}

int get_key_range(char const *s, int idsec, genalloc *ganocheck)
{
	int r,rn,rk ;
	
	stralloc tmp = STRALLOC_ZERO ;
	stralloc kp = STRALLOC_ZERO ;

	keynocheck nocheck = KEYNOCHECK_ZERO ;
	key_all_t const *list = total_list ;
	
	for (int i = 0;i<total_list_el[idsec];i++)
	{	
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
						
			rk = get_cleankey(tmp.s) ;
			if (!rk) return 0 ;
			
			if (!obstr_equal(tmp.s,list[idsec].list[i].name))
				continue ;
			
			rk = get_len_until(s+r,'\n') ;
			if (rk < 0) return 0 ;
			rk++;//remove the last '\n' character
		}
		
		if (!stralloc_cats(&kp,s+r+rk)) retstralloc(0,"get_key_range") ;
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
			if (!stralloc_cats(&nocheck.val, s+r)) retstralloc(0,"get_key_range") ;
			if (!stralloc_0(&nocheck.val)) retstralloc(0,"get_key_range") ;
		}
		else{
			if (!stralloc_catb(&nocheck.val, s+r,rk+rn)) retstralloc(0,"get_key_range") ;
			if (!stralloc_0(&nocheck.val)) retstralloc(0,"get_key_range") ;
		}
		if (!genalloc_append(keynocheck,ganocheck,&nocheck)) retstralloc(0,"get_key_range") ;
		
		nocheck = keynocheck_zero ;
	}
	if (!genalloc_len(keynocheck,ganocheck)) return 0 ;
	stralloc_free(&tmp) ;
	stralloc_free(&kp) ;

	return 1 ;
}

int get_keystyle(keynocheck *nocheck)
{
	/** Remove key name on nocheck->val*/
	if (nocheck->idsec != ENV){
		char tmp[nocheck->val.len + 1] ;
		memcpy(tmp,nocheck->val.s,nocheck->val.len) ;
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
				VERBO3 strerr_warnw2x("unknow format : ",get_keybyid(nocheck->expected)) ;
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
				strerr_warnw2x("unknow mandatory type: ",get_keybyid(list[idsec].list[idkey].mandatory)) ; 
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
					if (!keep_common(service,nocheck))
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
					if (!keep_common(service,nocheck))
					{
						VERBO3 strerr_warnwu1x("keep environ") ;
						return 0 ;
					} 
				break ;
			case SKIP:
				break ;
			default:
				VERBO3 strerr_warnw1x("unknow action") ;
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
	int idsec, svtype ;
	
	genalloc section = GENALLOC_ZERO ;//stra_keyval type
	genalloc nocheck = GENALLOC_ZERO ;//keynocheck type
		
	for (int i = 0 ; i<key_enum_section_el;i++)
		if (!get_section_range(str,i,&section)) return -4 ;
	
	for (unsigned int i = 0;i < genalloc_len(strakeyval,&section);i++)
	{
		idsec = get_enumbyid(gaikvkey(&section,i),key_enum_section_el) ;
		if (!get_key_range(gaikvval(&section,i),idsec,&nocheck)) return -3 ;
	}
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		if (!get_keystyle(&(genalloc_s(keynocheck,&nocheck)[i]))) return -2 ;
	}
	svtype = get_enumbyid(genalloc_s(keynocheck,&nocheck)[0].val.s,key_enum_el) ;
	if (svtype < 0) return -5 ;
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		idsec = genalloc_s(keynocheck,&nocheck)[i].idsec ;
		
		for (int j = 0;j < total_list_el[idsec] && total_list[idsec].list > 0;j++)
			if (!get_mandatory(&nocheck,idsec,j)) return -1 ;
	}
	
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&nocheck);i++)
	{
		if (!nocheck_toservice(&(genalloc_s(keynocheck,&nocheck)[i]),svtype,service)) return 0 ;
	}
	if ((service->opts[1]) && (svtype == LONGRUN))
	{
		if (!add_pipe(service, &deps))
		{
			VERBO3 strerr_warnwu2x("add pipe: ", keep.s+service->cname.name) ;
			return 0 ;
		} 
	}
	genalloc_free(stralist,&section) ;
	genalloc_free(keynocheck,&nocheck) ;

	return 1 ;
}

			
