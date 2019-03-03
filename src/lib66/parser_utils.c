/* 
 * parser_utils.c
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
#include <66/config.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>//MYUID

#include <string.h>
#include <unistd.h>//getuid
#include <stdlib.h>

#include <oblibs/bytes.h>
#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/strakeyval.h>

#include <skalibs/sig.h>
#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/diuint32.h>

#include <stdio.h>

void sv_alltype_free(sv_alltype *sv) 
{
	genalloc_free(diuint32,&sv->env) ;
	*&sv->env = genalloc_zero ;
}
void keynocheck_free(keynocheck *nocheck) 
{
	stralloc_free(&nocheck->val) ;
	*nocheck = keynocheck_zero ; 
}

ssize_t get_sep_before (char const *line, char const sepstart, char const sepend)
{
	size_t linend, linesep ;
	linesep=get_len_until(line,sepstart) ;
	linend=get_len_until(line,sepend) ;
	if (linesep > linend) return -1 ;
	if (!linend) return 0 ;
	return linesep ;
}

/**Tools for key*/
ssize_t scan_keybyname(char const *key,key_description_t const *list)
{
	for(unsigned int i = 0;list[i].name != 0;i++)
		if (obstr_equal(key,list[i].name)) return i ;
	
	return -1 ;
}

int scan_key(char const *s)
{
	ssize_t end ;
	end = get_sep_before(s,'=','\n') ;
	return 	(!end) ? -2 :
			(end < 0) ? -1 :
			end ;
}

int get_cleankey(char *s)
{
	ssize_t end ;
	char const *k ;
	k = s ;
	end = scan_key(k) ;
	if (end < 0) return 0 ;
	s[end] = 0 ;
	if (!obstr_trim(s,' ')) return 0 ;
	return 1 ;
}

int get_cleanval(char *s)
{

	ssize_t end,slen,plen,nlen ;
	end = slen = plen = nlen = 0 ;
	slen = strlen(s) ;
	char k[slen + 1] ;
	end = scan_key(s) ;

	if (end < 0) return 0 ;
	end++ ;
	plen = slen - end ;
	memcpy(k,s+end,plen) ;
	k[plen] = 0 ;
	nlen = strlen(k) ;
	byte_tozero(s,slen) ;
	memcpy(s,k,nlen) ;

	return 1 ;
}

int get_keyline(char const *s,char const *key,stralloc *sa)
{
	int pos ;
	size_t stmp ;
	size_t rs, re ;
	size_t keylen = strlen(key) ;
	size_t slen = strlen(s) ;
	char buff[slen + 1] ;
	pos = rs = re = 0 ;

	if (slen < keylen) return -1 ;
	rs = byte_search(s,slen,key,keylen) ;
	if (rs+keylen > slen) return -1 ;
	rs = get_rlen_until(s,'\n',rs) ;
	/** beginning of @s*/
	if (rs < 0){
		rs = 0 ;
	}else pos = rs + 1 ; //remove the rlen character
	re  = get_len_until(s+pos,'\n') ;
	if (!re) return -1 ;
	stmp = slen - ((slen - re)) ;
	memcpy(buff,s+pos,stmp) ;
	buff[re] = 0 ;
	sa->len = 0 ;
	if (!stralloc_cats(sa,buff)) retstralloc(-1,"get_keyline") ;
	if (!stralloc_0(sa)) retstralloc(-1,"get_keyline") ;
	
	return pos ;
}

int get_key(char const *s,char const *key,stralloc *keep)
{
	int pos ;
	pos = get_keyline(s,key,keep) ;
	return (pos < 0) ? -1 : pos ;
}

int get_nextkey(char const *val, int idsec, const key_description_t *list)
{
	
	int pos, newpos,wpos ;
	size_t vlen, end ;
	
	stralloc tmp = STRALLOC_ZERO ;

	vlen = strlen(val) ;
	char key[vlen + 1] ;
	pos = newpos = 0 ;

	pos = get_key(val,"=",&tmp) ;
	/** end of string*/
	if (pos < 0)
	{
		stralloc_free(&tmp) ;
		return pos ;
	}
	strcpy(key,tmp.s) ;
	get_cleankey(key) ;
	size_t keylen = strlen(key) ;
	
	loop:	
		end = get_len_until(val+pos,'\n') ;
		end++;
		newpos = pos ;
		/** We don't know the name of the key
		 * in the environment section, skip it*/
		for (int i = 0 ; i < total_list_el[idsec]; i++)
		{
			for (wpos=0;wpos<keylen;wpos++)
			{
				if (key[wpos] == '@') break ; 
			}
			if(list[i].name)
				if (obstr_equal(key+wpos,list[i].name))
				{
					stralloc_free(&tmp) ;
					return newpos ; 
				}
		}
		pos = get_key(val+newpos+end,"=",&tmp) ;
		/** end of string*/
		if (pos < 0)
		{
			stralloc_free(&tmp) ;
			return pos ;//+ newpos ;
		}
		strcpy(key,tmp.s) ;
		get_cleankey(key) ;
		
		pos = pos + newpos + end ;
		goto loop ;
	
	stralloc_free(&tmp) ;
	
	return pos ;
}

/** Tools for section */
ssize_t scan_section(char const *s)
{
	ssize_t end ;
	end = 0 ;
	
	if (s[0] == '[') end = get_len_until(s,']') ;
	return end<0 ? -1 : end ;
}

int get_cleansection(char const *s,stralloc *sa)
{
	ssize_t end ;
	size_t slen = strlen(s) ;
	int r, ierr ;
	r = ierr = 0 ;
	char tmp[slen + 1] ;
	r = scan_isspace(s) ;
	end = scan_section(s+r) ;
	
	if(end>0){
		memcpy(tmp,s+r+1,end -1);
		tmp[end-1] = 0 ;
		//if (!stralloc_catb(sa,s+r+1,end - 1 )) retstralloc(-1,"get_cleansection") ;
	}else{
		memcpy(tmp,s+r+1,slen) ;
		tmp[slen] = 0 ;
		//if (!stralloc_catb(sa,s+r+1,strlen(s))) retstralloc(-1,"get_cleansection") ;
	}
	if (!stralloc_cats(sa,tmp)) retstralloc(-1,"get_cleansection") ;
	if (!stralloc_0(sa)) retstralloc(-1,"get_cleansection:stralloc_0") ;
	r = get_enumbyid(sa->s,key_enum_section_el) ;
	if (end<0){
		sa->len = 0 ;
		return -4 ;
	}
	if (!end){
		sa->len = 0 ;
		return -1 ;
	}
	if (r < 0){
		sa->len = 0 ;
		return -2 ;
	}
	return 1 ;
}			


int parse_line(keynocheck *nocheck)
{
	char *val ;
	ssize_t end ;
	
	end = get_len_until(nocheck->val.s,'\n') ;
	if (end < 0) return 0 ;
	
	char kp[end + 1] ;
	memcpy(kp,nocheck->val.s, end) ;
	kp[end] = 0 ;
	
	val = kp ;
	obstr_trim(val,' ') ;
	obstr_trim(val,'\n') ;
	if (!stralloc_obreplace(&nocheck->val,val)) return 0 ;
	/** we don't accept empty value*/
	if (!nocheck->val.len)
	{
		VERBO3 strerr_warnw2x("empty value for key: ",get_keybyid(nocheck->idkey)) ;
		return 0 ;
	}
	return 1 ;
}

int parse_quote(keynocheck *nocheck)
{
	char *val ;
	ssize_t end, r ;
	
	end = 0 ;
	end = get_len_until(nocheck->val.s,'\n') ;
	if (end < 0) return 0 ;
	
	char kp[end + 1] ;
	memcpy(kp,nocheck->val.s, end) ;
	kp[end] = 0 ;
	
	val = kp ;
	r = scan_isspace(val) ;
	obstr_trim(val,'"') ;
	obstr_trim(val,'\n') ;
	if (!stralloc_obreplace(&nocheck->val,val+r)) return 0 ;
	
	/** we don't accept empty value*/
	if (!nocheck->val.len)
	{
		VERBO3 strerr_warnw2x("empty value for key: ",get_keybyid(nocheck->idkey)) ;
		return 0 ;
	}
	return 1 ;
}

int parse_bracket(keynocheck *nocheck)
{
	
	ssize_t start,end,nl ;
	size_t nlen ;
	char tmp[nocheck->val.len + 1] ;
	start = end = nl = 0 ;
	nlen = nocheck->val.len - 1 ;
	start = get_len_until(nocheck->val.s,'(') ;
	if (start < 0) return 0 ;
	start++;
	end = get_rlen_until(nocheck->val.s,')',nlen) ;
	
	if (end < 0) return 0 ;
	memcpy(tmp,nocheck->val.s+start,end  - start) ;
	tmp[end - start] = 0 ;
	//obstr_replace(tmp,'\n',' ') ;
	
	if (!stralloc_obreplace(&nocheck->val, tmp)) return 0 ;
	
	/** we don't accept empty value*/
	if (!nocheck->val.len)
	{
		VERBO3 strerr_warnw2x("empty value for key: ",get_keybyid(nocheck->idkey)) ;
		return 0 ;
	}
	
	return 1 ;
}

int parse_env(keynocheck *nocheck)
{

	int r, nbline, pos ;
	size_t base ;
	char buf[nocheck->val.len + 1 + 1] ;

	stralloc kp = STRALLOC_ZERO ;
	genalloc trunc = GENALLOC_ZERO ; //stralist

	nbline = get_nbline_ga(nocheck->val.s,nocheck->val.len,&trunc) ;
	pos = base = 0 ;

	while(pos < nbline){
		byte_tozero(buf,nocheck->val.len + 1) ;
		if (!get_wasted_line(gaistr(&trunc,pos)))
		{
			pos++ ;
			continue ;
		}
		if(!gaistrlen(&trunc,pos))
		{
			pos++ ;
			continue ;
		}
		memcpy(buf,gaistr(&trunc,pos),gaistrlen(&trunc,pos)) ;
		r = get_sep_before(buf,'=','\n') ;
		if (r < 0)
		{ 
			pos++ ;
			continue ;
		}
	
		if (!stralloc_cats(&kp,buf)) retstralloc(0,"parse_env") ;
		pos++;
	}
	
	if (!stralloc_0(&kp)) retstralloc(0,"parse_env") ;
	if (!stralloc_obreplace(&nocheck->val, kp.s)) return 0 ;
	/** we don't accept empty value*/
	if (!nocheck->val.len)
	{
		VERBO3 strerr_warnw2x("empty value for key: ",get_keybyid(nocheck->idkey)) ;
		return 0 ;
	}
	genalloc_deepfree(stralist,&trunc,stra_free) ;
	stralloc_free(&kp) ;
	
	return 1 ;
}
void parse_err(int ierr,int idsec,int idkey)
{
	switch(ierr)
	{
		case 0: 
			strerr_warnw4x("invalid value for key : ",get_keybyid(idkey)," : in section : ",get_keybyid(idsec)) ;
			break ;
		case 1:
			strerr_warnw4x("multiple definition of key : ",get_keybyid(idkey)," : in section : ",get_keybyid(idsec)) ;
			break ;
		case 2:
			strerr_warnw4x("same value for key : ",get_keybyid(idkey)," : in section : ",get_keybyid(idsec)) ;
			break ;
		case 3:
			strerr_warnw4x("key : ",get_keybyid(idkey)," : must be an integrer value in section : ",get_keybyid(idsec)) ;
			break ;
		case 4:
			strerr_warnw4x("key : ",get_keybyid(idkey)," : must be an absolute path in section : ",get_keybyid(idsec)) ;
			break ;
		case 5:
			strerr_warnw4x("key : ",get_keybyid(idkey)," : must be set in section : ",get_keybyid(idsec)) ;
			break ;
		default:
			strerr_warnw1x("unknow parse_err number") ;
			break ;
	}
}
int scan_nbsep(char *line,int lensearch, char const sepstart, char const sepend)
{
	ssize_t start, end ;
	start = byte_tosearch(line,lensearch,sepstart) ;
	end = byte_tosearch(line,lensearch,sepend) ;
	if (start > end ) return -1 ; /**unmatched ( */
	if (start < end ) return -2 ; /**unmatched ) */
	if ((!start) && (!end)) return 0 ; /** find nothing*/
	return 1 ;
}
void sep_err(int r,char const sepstart,char const sepend,char const *keyname)
{
	if (r == -1) strerr_warnw4x("unmatched ", &sepstart," at key : ",keyname) ;
	if (r == -2) strerr_warnw4x("unmatched ", &sepend," at key : ",keyname) ;
	if (!r) strerr_warnw2x("bad syntax at key : ",keyname) ;
}

int add_env(char *line,genalloc *ga,stralloc *sa)
{
	unsigned int i = 0, err = 1 ;
	
	char *k = 0 ;
	char *v = 0 ;
	
	genalloc gatmp = GENALLOC_ZERO ;
	stralloc satmp = STRALLOC_ZERO ;
	diuint32 tmp = DIUINT32_ZERO ;
	
	if (!get_wasted_line(line)) goto freed ;
	k = line ;
	v = line ;
	obstr_sep(&v,"=") ;
	if (v == NULL) { err = 0 ; goto freed ; }
	
	if (!clean_val(&gatmp,k)) { err = 0 ; goto freed ; }
	for (i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
	{
		if ((i+1) < genalloc_len(stralist,&gatmp))
		{
			if (!stralloc_cats(&satmp,gaistr(&gatmp,i))) { err = 0 ; goto freed ; }
			if (!stralloc_cats(&satmp," ")) { err = 0 ; goto freed ; }
		}
		else if (!stralloc_catb(&satmp,gaistr(&gatmp,i),gaistrlen(&gatmp,i)+1)) { err = 0 ; goto freed ; }
	}
	tmp.left = sa->len ;
	if(!stralloc_catb(sa,satmp.s,satmp.len+1)) { err = 0 ; goto freed ; }
	
	if (!obstr_trim(v,'\n')) { err = 0 ; goto freed ; }
	satmp.len = 0 ;
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	if (!clean_val(&gatmp,v)) { err = 0 ; goto freed ; }
	for (i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
	{
		if ((i+1) < genalloc_len(stralist,&gatmp))
		{
			if (!stralloc_cats(&satmp,gaistr(&gatmp,i))) { err = 0 ; goto freed ; }
			if (!stralloc_cats(&satmp," ")) { err = 0 ; goto freed ; }
		}
		else if (!stralloc_catb(&satmp,gaistr(&gatmp,i),gaistrlen(&gatmp,i)+1)) { err = 0 ; goto freed ; }
	}
	tmp.right = sa->len ;
	if(!stralloc_catb(sa,satmp.s,satmp.len+1)) { err = 0 ; goto freed ; }
	
	if (!genalloc_append(diuint32,ga,&tmp)) err = 0 ;
		
	freed:
		stralloc_free(&satmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return err ;
}
int add_pipe(sv_alltype *sv, stralloc *sa)
{
	char *prodname = keep.s+sv->cname.name ;
	
	stralloc tmp = STRALLOC_ZERO ;

	sv->pipeline = sa->len ;
	if (!stralloc_cats(&tmp,SS_PIPE_NAME)) retstralloc(0,"add_pipe") ;
	if (!stralloc_cats(&tmp,prodname)) retstralloc(0,"add_pipe") ;
	if (!stralloc_0(&tmp)) retstralloc(0,"add_pipe") ;
	
	if (!stralloc_catb(sa,tmp.s,tmp.len+1)) retstralloc(0,"add_pipe") ;
	
	stralloc_free(&tmp) ;
	
	return 1 ;
}
int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype)
{
	int r, nbline ;
	unsigned int i ;
	genalloc gatmp = GENALLOC_ZERO ;
	r = i = 0 ;

	switch(nocheck->idkey){
		case TYPE:
			r = get_enumbyid(nocheck->val.s,key_enum_el) ;
			if (r < 0)
			{
				VERBO3 parse_err(0,nocheck->idsec,TYPE) ;
				return 0 ;
			}
			service->cname.itype = r ;
			break ;
		case NAME:
			service->cname.name = keep.len ;
			if (!stralloc_catb(&keep,nocheck->val.s,nocheck->val.len + 1)) retstralloc(0,"parse_common") ;
			break ;
		case DESCRIPTION:
			service->cname.description = keep.len ;
			if (!stralloc_catb(&keep,nocheck->val.s,nocheck->val.len + 1)) retstralloc(0,"parse_common") ;
			break ;
		case OPTIONS:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			for (i = 0;i<genalloc_len(stralist,&gatmp);i++)
			{
				r = get_enumbyid(gaistr(&gatmp,i),key_enum_el) ;
				if (r < 0)
				{
					VERBO3 parse_err(0,nocheck->idsec,OPTIONS) ;
					return 0 ;
				}
				if (svtype == CLASSIC || svtype == LONGRUN)
				{
					if (r == LOGGER)
						service->opts[0] = 1 ;/**0 means not enabled*/
					else if (svtype == LONGRUN && r == PIPELINE)
						service->opts[1] = 1 ;
				}
				if (r == ENVIR)
					service->opts[2] = 1 ;
				else if (r == DATA)
					service->opts[3] = 1 ;
			}
			
			break ;
		case FLAGS:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			for (unsigned int i = 0;i<genalloc_len(stralist,&gatmp);i++)
			{
				r = get_enumbyid(gaistr(&gatmp,i),key_enum_el) ;
				if (r < 0)
				{
					VERBO3	parse_err(0,nocheck->idsec,FLAGS) ;
					return 0 ;
				}
				if (r == DOWN) 
					service->flags[0] = 1 ;/**0 means not enabled*/				
				if (r == NOSETSID)
					service->flags[1] = 1 ;
			}
			break ;
		case USER:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			/** special case, we don't know which user want to use
			 * the service, we need a general name to allow all user
			 * the term "user" is took here to allow the current user*/
			if (stra_cmp(&gatmp,"user"))
			{
				stra_remove(&gatmp,"user") ;
				for (i=0;i<genalloc_len(stralist,&gatmp);i++)
				{
					r = scan_uidlist(gaistr(&gatmp,i),(uid_t *)service->user) ;
					if (!r)
					{
						VERBO3	parse_err(0,nocheck->idsec,USER) ;
						return 0 ;
					}
				}
				uid_t nb = service->user[0] ;
				nb++ ;
				service->user[0] = nb ;
				service->user[nb] = MYUID ;
				//search for root permissions
				int e = 1 ;
				for (int i =1; i < nb; i++)
					if (!service->user[i]) e = 0 ;
				if ((!MYUID) && (e))
				{
					VERBO3 strerr_warnwu3x("use ",keep.s+service->cname.name," service: permission denied") ;
					return 0 ;
				}
				break ;
			}
			else
			if (MYUID > 0)
			{
				VERBO3 strerr_warnwu3x("use ",keep.s+service->cname.name," service: permission denied") ;
				return 0 ;
			}
			else if (genalloc_len(stralist,&gatmp) > 1)
			{
				r = scan_uidlist(nocheck->val.s,(uid_t *)service->user) ;
				if (!r)
				{
					VERBO3 parse_err(0,nocheck->idsec,USER) ;
					return 0 ;
				}
				break ;
			}
			else
			r = scan_uidlist("root",(uid_t *)service->user) ;
			if (!r)
			{
				VERBO3 parse_err(0,nocheck->idsec,USER) ;
				return 0 ;
			}
			break ;
		case DEPENDS:
			if (service->cname.itype == CLASSIC)
			{
				VERBO3 strerr_warnw3x("key : ",get_keybyid(nocheck->idkey)," : is not valid for type classic") ;
				return 0 ;
			}
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			service->cname.idga = genalloc_len(unsigned int,&gadeps) ;
			for (i = 0;i<genalloc_len(stralist,&gatmp);i++)
			{
				if (!genalloc_append(unsigned int,&gadeps,&deps.len)) retstralloc(0,"parse_common") ;
				if (!stralloc_catb(&deps,gaistr(&gatmp,i),gaistrlen(&gatmp,i) + 1)) retstralloc(0,"parse_common") ;
				service->cname.nga++ ;
			}
			break ;
		case CONTENTS:
			if (service->cname.itype == CLASSIC)
			{
				VERBO3 strerr_warnw3x("key : ",get_keybyid(nocheck->idkey)," : is not valid for type classic") ;
				return 0 ;
			}
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			service->cname.idga = genalloc_len(unsigned int,&gadeps) ;
			for (i = 0;i<genalloc_len(stralist,&gatmp);i++)
			{
				if (!genalloc_append(unsigned int,&gadeps,&deps.len)) retstralloc(0,"parse_common") ;
				if (!stralloc_catb(&deps,gaistr(&gatmp,i),gaistrlen(&gatmp,i) + 1)) retstralloc(0,"parse_common") ;
				service->cname.nga++ ;
			}
			break ;
		case NOTIFY:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			if (!scan_uint32(gastr(&gatmp)))
			{
				VERBO3 parse_err(3,nocheck->idsec,NOTIFY) ;
				return 0 ;
			}
			if (!uint32_scan(gastr(&gatmp),&service->notification))
			{
				VERBO3 parse_err(3,nocheck->idsec,NOTIFY) ;
				return 0 ;
			}
			break ;
		case T_KILL:
			r = scan_timeout(nocheck->val.s,(uint32_t *)service->timeout,0) ;
			if (r < 0)
			{
				VERBO3 parse_err(3,nocheck->idsec,T_KILL) ;
				return 0 ;
			}
			break ;
		case T_FINISH:
			r = scan_timeout(nocheck->val.s,(uint32_t *)service->timeout,1) ;
			if (r < 0)
			{
				VERBO3 parse_err(3,nocheck->idsec,T_FINISH) ;
				return 0 ;
			}
			break ;
		case T_UP:
			r = scan_timeout(nocheck->val.s,(uint32_t *)service->timeout,2) ;
			if (r < 0)
			{
				VERBO3 parse_err(3,nocheck->idsec,T_UP) ;
				return 0 ;
			}
			break ;
		case T_DOWN:
			r = scan_timeout(nocheck->val.s,(uint32_t *)service->timeout,3) ;
			if (r < 0)
			{
				VERBO3 parse_err(3,nocheck->idsec,T_DOWN) ;
				return 0 ;
			}
			break ;
		case DEATH:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			if (!scan_uint32(gastr(&gatmp)))
			{
				VERBO3 parse_err(3,nocheck->idsec,DEATH) ;
				return 0 ;
			}
			if (!uint32_scan(gastr(&gatmp),&service->death))
			{
				VERBO3 parse_err(3,nocheck->idsec,DEATH) ;
				return 0 ;
			}
			break ;
		case ENVAL:
			nbline = get_nbline_ga(nocheck->val.s,nocheck->val.len,&gatmp) ;
			for (i = 0;i < nbline;i++)
			{
				if (!add_env(gaistr(&gatmp,i),&service->env,&saenv))
				{
					VERBO3 strerr_warnwu2x("add environment value: ",gaistr(&gatmp,i)) ;
					return 0 ;
				}
			}
			break ;
		case SIGNAL:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			if (!sig0_scan(gastr(&gatmp), &service->signal))
			{
				VERBO3 parse_err(3,nocheck->idsec,SIGNAL) ;
				return 0 ;
			}
			break ;
		default:
			VERBO3 strerr_warnw2x("unknow key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
}

int keep_runfinish(sv_exec *exec,keynocheck *nocheck)
{
	int r ;
	
	genalloc gatmp = GENALLOC_ZERO ;
	
	switch(nocheck->idkey){
		case BUILD:
			r = get_enumbyid(nocheck->val.s,key_enum_el) ;
			if (r < 0)
			{
				VERBO3 parse_err(0,nocheck->idsec,BUILD) ;
				return 0 ;
			}
			exec->build = r ;
			break ;
		case RUNAS:
			r = scan_uid(nocheck->val.s,&exec->runas) ;
			if (!r)
			{
				VERBO3 parse_err(0,nocheck->idsec,RUNAS) ;
				return 0 ;
			}
			break ;
		case SHEBANG:
			r = dir_scan_absopath(nocheck->val.s) ;
			if (r < 0)
			{
				VERBO3 parse_err(4,nocheck->idsec,SHEBANG) ;
				return 0 ;
			}
			exec->shebang = keep.len ;
			if (!stralloc_catb(&keep,nocheck->val.s,nocheck->val.len + 1)) exitstralloc("parse_runfinish:stralloc:SHEBANG") ;
			break ;
		case EXEC:
			exec->exec = keep.len ;
			if (!stralloc_catb(&keep,nocheck->val.s,nocheck->val.len + 1)) exitstralloc("parse_runfinish:stralloc:EXEC") ;
			
			break ;
		default:
			VERBO3 strerr_warnw2x("unknow key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
		}

	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
}
int keep_logger(sv_execlog *log,keynocheck *nocheck)
{
	int r ;

	genalloc gatmp = GENALLOC_ZERO ;

	switch(nocheck->idkey){
		case BUILD:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case RUNAS:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case SHEBANG:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case EXEC:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case T_KILL:
			r = scan_timeout(nocheck->val.s,(uint32_t *)log->timeout,0) ;
			if (r < 0) parse_err(3,nocheck->idsec,T_KILL) ;
			break ;
		case T_FINISH:
			r = scan_timeout(nocheck->val.s,(uint32_t *)log->timeout,1) ;
			if (r < 0)
			{
				VERBO3 parse_err(3,nocheck->idsec,T_FINISH) ;
				return 0 ;
			}
			break ;
		case DESTINATION:
			r = dir_scan_absopath(nocheck->val.s) ;
			if (r < 0)
			{
				VERBO3 parse_err(4,nocheck->idsec,DESTINATION) ;
				return 0 ;
			}
			log->destination = keep.len ;
			if (!stralloc_catb(&keep,nocheck->val.s,nocheck->val.len + 1)) retstralloc(0,"parse_logger") ;
			break ;
		case BACKUP:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			if (!scan_uint32(gastr(&gatmp)))
			{
				VERBO3 parse_err(3,nocheck->idsec,BACKUP) ;
				return 0 ;
			}
			uint_scan(gastr(&gatmp),&log->backup) ;
			break ;
		case MAXSIZE:
			if (!scan_uint32(nocheck->val.s))
			{
				VERBO3 parse_err(3,nocheck->idsec,MAXSIZE) ;
				return 0 ;
			}
			uint_scan(nocheck->val.s,&log->maxsize) ;
			break ;
		case TIMESTP:
			r = get_enumbyid (nocheck->val.s,key_enum_el) ;
			if (r < 0)
			{
				VERBO3 parse_err(0,nocheck->idsec,TIMESTP) ;
				return 0 ;
			}
			log->timestamp = r ;
			break ;
		default:
			VERBO3 strerr_warnw2x("unknow key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
	}
	
	genalloc_deepfree (stralist,&gatmp,stra_free) ;
	
	return 1 ;
}

