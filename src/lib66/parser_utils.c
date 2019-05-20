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

#include <string.h>
#include <unistd.h>//getuid
#include <stdlib.h>
#include <stdio.h>

#include <oblibs/bytes.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/strakeyval.h>

#include <skalibs/sig.h>
#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/diuint32.h>
#include <skalibs/djbunix.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>//MYUID
#include <66/environ.h>//MYUID

stralloc keep = STRALLOC_ZERO ;//sv_alltype data
stralloc deps = STRALLOC_ZERO ;//sv_name depends
stralloc saenv = STRALLOC_ZERO ;//sv_alltype env

genalloc ganame = GENALLOC_ZERO ;//sv_name_t, avltree
genalloc gadeps = GENALLOC_ZERO ;//unsigned int, pos in deps

genalloc gasv = GENALLOC_ZERO ;//sv_alltype general

/**********************************
 *		freed function
 * *******************************/
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

void section_free(section_t *sec)
{
	stralloc_free(&sec->main) ;
	stralloc_free(&sec->start) ;
	stralloc_free(&sec->stop) ;
	stralloc_free(&sec->logger) ;
	stralloc_free(&sec->environment) ;	
}

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
/**********************************
 *		parser utilities
 * *******************************/
int parse_line(stralloc *src, size_t *pos)
{
	size_t newpos = 0 ;
	stralloc kp = STRALLOC_ZERO ;
	char const *file = "parse_line" ;
	parse_mill_t line = { .open = '@', .close = '\n', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
					
	stralloc_inserts(src,0,"@") ;
	if (!parse_config(&line,file,src,&kp,&newpos)) goto err ;
	if (!stralloc_0(&kp)) goto err ;
	if (!clean_value(&kp)) goto err ;
	if (!stralloc_copy(src,&kp)) goto err ;
	*pos += newpos - 1 ;
	stralloc_free(&kp) ;
	return 1 ;
	err:
		stralloc_free(&kp) ;
		return 0 ;
}

int parse_quote(stralloc *src,size_t *pos)
{
	int r, err = 0 ;
	size_t newpos = 0 ;
	char const *file = "parse_quote" ;
	stralloc kp = STRALLOC_ZERO ;
	parse_mill_t quote = { .open = '"', .close = '"', \
							.skip = 0, .skiplen = 0, \
							.end = "\n", .endlen = 1, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 0, \
							.inner = PARSE_MILL_INNER_ZERO } ;

	r = parse_config(&quote,file,src,&kp,&newpos) ;
	if (!r) goto err ;
	else if (r == -1) { err = -1 ; goto err ; }
	if (!stralloc_0(&kp)) goto err ;
	if (!stralloc_copy(src,&kp)) goto err ;
	*pos += newpos - 1 ;
	stralloc_free(&kp) ;
	return 1 ;
	err:
		stralloc_free(&kp) ;
		return err ;
}

int parse_bracket(stralloc *src,size_t *pos)
{
	int err = -1 ;
	size_t newpos = 0 ;
	ssize_t id = -1, start = -1, end = -1 ;
	char const *file = "parse_bracket" ;
	stralloc kp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	
	parse_mill_t key = { .open = '@', .close = '=', \
							.skip = " \n\t\r", .skiplen = 4, \
							.end = 0, .endlen = 0, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 1, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	
	while (id < 0 && newpos < src->len) 
	{
		kp.len = 0 ;
		key.inner.nclose = 0 ;
		key.inner.nopen = 0 ;
		if (!parse_config(&key,file,src,&kp,&newpos)) goto err ;
		if (!kp.len) break ;//end of string
		if (!stralloc_0(&kp)) goto err ;
		if (!clean_value(&kp)) goto err ;
		if (!stralloc_inserts(&kp,0,"@")) goto err ;
		id = get_enumbyid(kp.s,key_enum_el) ;
	}
	/** id is negative and we are at the end of string
	 * field can contain instance name with @ character
	 * in this case we keep the src as it */
	if (id < 0)
	{
		if (!stralloc_cats(&tmp,src->s)) goto err ;
	}else if (!stralloc_catb(&tmp,src->s,(newpos - (kp.len+1)))) goto err ;//+1 remove the @ of the next key
	if (!stralloc_0(&tmp)) goto err ;
	kp.len = 0 ;
	start = get_len_until(tmp.s,'(') ;
	if (start < 0) { err = -1 ; goto err ; }
	start++ ;
	end = get_rlen_until(tmp.s,')',tmp.len) ;
	if (end < 0) { err = -1 ; goto err ; }
	if (!stralloc_catb(&kp,tmp.s+start,end-start)) goto err ;
	if (!stralloc_0(&kp)) goto err ;
	if (!stralloc_copy(src,&kp)) goto err ;
	*pos += end + 1 ; //+1 to remove the last ')'
	stralloc_free(&kp) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		return err ;
}

int parse_env(stralloc *src,size_t *pos)
{
	int r ;
	size_t newpos = 0 ;
	size_t base ;
	stralloc kp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	char const *file = "parse_env" ;
	parse_mill_t line = { .open = '@', .close = '\n', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	size_t blen = src->len, n = 0 ;
	if (!stralloc_inserts(src,0,"@")) goto err ;
	while(newpos < (blen+n))
	{
		base = kp.len ;
		line.inner.nopen = line.inner.nclose = 0 ;
		r = parse_config(&line,file,src,&kp,&newpos) ;
		if (!r) goto err ;
		else if (r < 0){ kp.len = base ; goto append ; }
		if (!stralloc_cats(&kp,"\n")) goto err ;
		append:
		if (!stralloc_inserts(src,newpos,"@")) goto err ;
		n++;
	}
	if (!stralloc_0(&kp)) goto err ;
	if (!stralloc_copy(src,&kp)) goto err ;
	*pos += newpos - 1 ;
	stralloc_free(&kp) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

/**********************************
 *		parser split function
 * *******************************/

int get_section_range(section_t *sasection,stralloc *src)
{
	size_t pos = 0, start = 0 ;
	int n = 0, id = -1, /*idc = 0,*/ skip = 0, err = 0 ;
	stralloc secname = STRALLOC_ZERO ;
	stralloc_ref psasection ;
	parse_mill_t section = { .open = '[', .close = ']', \
							.skip = " \n\t\r", .skiplen = 4, \
							.end = 0, .endlen = 0, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 1, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	while (pos < src->len)
	{
		if(secname.len && n)
		{
			skip = section_skip(src->s,pos,section.inner.nline) ;
			id = get_enumbyid(secname.s,key_enum_section_el) ;
			section_setsa(id,&psasection,sasection) ;
			if (skip) sasection->idx[id] = 1 ;
		}
		secname.len = section.inner.nclose = section.inner.nopen = 0 ;
		id = -1 ;
		while (id < 0 && pos < src->len) 
		{
			secname.len = section.inner.nclose = section.inner.nopen = 0 ;
			if(!parse_config(&section,sasection->file,src,&secname,&pos)) goto err ;
			if (!secname.len) break ; //end of string
			if (!stralloc_0(&secname)) goto err ;
			id = get_enumbyid(secname.s,key_enum_section_el) ;
			/*if (id < 0)
			{ 
				idc = section_valid(id,section.inner.nline,pos,src,sasection->file) ;
				if (!idc) goto err ;
				else if (idc < 0) goto invalid ;
			}*/
		}
		if (!n)
		{ 
			skip = section_skip(src->s,pos,section.inner.nline) ;
			section_setsa(id,&psasection,sasection) ;
			if (skip) sasection->idx[id] = 1 ;
			start = pos ;
			id = -1 ;
			while (id < 0 && pos < src->len)
			{	
				section.inner.nclose = section.inner.nopen = secname.len = 0 ;
				if (!parse_config(&section,sasection->file,src,&secname,&pos)) goto err ;
				if (!secname.len) break ; //end of string
				if (!stralloc_0(&secname)) goto err ;
				id = get_enumbyid(secname.s,key_enum_section_el) ;
				/*if (id < 0)
				{
					idc = section_valid(id,section.inner.nline,pos,src,sasection->file) ;
					if (!idc) goto err ;
					else if (idc < 0) goto invalid ;
				}*/
			}
			if(skip)
			{
				if (!stralloc_catb(psasection,src->s+start,(pos - start) - (secname.len + 2))) goto err ;//+2 to remove '[]'character
				if (!stralloc_0(psasection)) goto err ;
			}
			n++ ;
			start = pos ;
		}
		else
		{	
			if (skip)
			{
				if (!stralloc_catb(psasection,src->s+start,(pos - start) - (secname.len + 1))) goto err ;//only -1 to remove '\n'character
				if (!stralloc_0(psasection)) goto err ;
			}
			start = pos ;
		}
	}
	stralloc_free(&secname) ;
	return 1 ;
	/*invalid:
		err = -1 ;
		VERBO1 strerr_warnw2x("invalid section: ",secname.s) ;
	*/err:
		stralloc_free(&secname) ;
		return err ;
}

int get_key_range(genalloc *ga, section_t *sasection,char const *file,int *svtype)
{	
	int r ;
	size_t pos = 0 ;
	uint8_t found = 0 ;
	stralloc sakey = STRALLOC_ZERO ;
	stralloc_ref psasection ;
	key_all_t const *list = total_list ;
	parse_mill_t key = { .open = '@', .close = '=', \
							.skip = " \n\t\r", .skiplen = 4, \
							.end = 0, .endlen = 0, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 1, \
							.forceskip = 1, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
							
	for (int i = 0 ; i < key_enum_section_el ; i++)
	{	
		if (sasection->idx[i])
		{
			if (i == ENV)
			{	
				pos = 0 ;	
				keynocheck nocheck = KEYNOCHECK_ZERO ;
				nocheck.idsec = i ;
				nocheck.idkey = ENVAL ;
				nocheck.expected = KEYVAL ;
				nocheck.mandatory = OPTS ;
				section_setsa(i,&psasection,sasection) ;
				if (!stralloc_cats(&nocheck.val,psasection->s+1)) goto err ;
				if (!parse_env(&nocheck.val,&pos)) { strerr_warnwu2x("parse section: ",get_keybyid(i)) ; goto err ; }
				if (!genalloc_append(keynocheck,ga,&nocheck)) goto err ;
			} 
			else
			{
				section_setsa(i,&psasection,sasection) ;
				pos = 0 ;
				size_t blen = psasection->len ;
				while (pos < blen)
				{
					keynocheck nocheck = KEYNOCHECK_ZERO ;
					key.inner.nopen = key.inner.nclose = sakey.len = 0 ; 
					r = parse_config(&key,file,psasection,&sakey,&pos) ;
					if (!r) goto err ;
					if (!stralloc_cats(&nocheck.val,psasection->s+pos)) goto err ;
					if (!stralloc_0(&nocheck.val)) goto err ;
					if (!sakey.len) break ;// end of string
					stralloc_inserts(&sakey,0,"@") ;
					stralloc_0(&sakey) ;
					for (int j = 0 ; j < total_list_el[i]; j++)
					{
						found = 0 ;
						if (list[i].list[j].name && obstr_equal(sakey.s,list[i].list[j].name))
						{
							nocheck.idsec = i ;
							nocheck.idkey = get_enumbyid(sakey.s,key_enum_el) ;
							nocheck.expected = list[i].list[j].expected ;
							nocheck.mandatory = list[i].list[j].mandatory ;
							found = 1 ;
							switch(list[i].list[j].expected)
							{
								case QUOTE:
									r = parse_quote(&nocheck.val,&pos) ;
									if (r < 0)
									{
										VERBO3 parse_err(6,nocheck.idsec,nocheck.idkey) ;
										goto err ;
									}
									else if (!r) goto err ;
									break ;
								case BRACKET:
									r = parse_bracket(&nocheck.val,&pos) ;
									if (r < 0)
									{
										VERBO3 parse_err(6,nocheck.idsec,nocheck.idkey) ;
										goto err ;
									}
									else if (!r) goto err ;
									break ;
								case LINE:
								case UINT:
								case SLASH:
									if (!parse_line(&nocheck.val,&pos)) goto err ;
									if (!i && !j) (*svtype) = get_enumbyid(nocheck.val.s,key_enum_el) ;
									break ;
								default:
									return 0 ;
							}
							if (!genalloc_append(keynocheck,ga,&nocheck)) goto err ;
							break ;
						}
					}			 
					if (!found && r >=0) 
					{ 
						VERBO3 strerr_warnw4x("unknown key: ",sakey.s," : in section: ",get_keybyid(sasection->idx[i])) ; 
						keynocheck_free(&nocheck) ;
						goto err ; 
					}
				}
			}
		}
	}
	
	stralloc_free(&sakey) ;
	return 1 ;
	err: 
		stralloc_free(&sakey) ;
		return 0 ;
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
				VERBO3 strerr_warnw4x("mandatory key: ",list[idsec].list[idkey].name," not found on section: ",get_keybyid(idsec)) ;
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
			if ((!count) && (countidsec) && bkey>=0)
			{
				if (obstr_equal(genalloc_s(keynocheck,nocheck)[bkey].val.s,get_keybyid(CUSTOM)))
				{
					VERBO3 strerr_warnw5x("custom build asked on section: ",get_keybyid(idsec)," -- key: ",list[idsec].list[idkey].name," must be set") ;
					return 0 ;
				}
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
			if ((!count) && (countidsec) && bkey>=0)
			{
				if (obstr_equal(genalloc_s(keynocheck,nocheck)[bkey].val.s,get_keybyid(BUNDLE)))
				{
					VERBO3 strerr_warnw1x("bundle type detected -- key @contents must be set") ;
					return 0 ;
				}
			}
			break ;
		/** only pass through here to check if flags env was asked
		 * and the corresponding section exist*/	
		case OPTS:
			for (unsigned int j = 0;j < genalloc_len(keynocheck,nocheck);j++)
			{
				
				if (genalloc_s(keynocheck,nocheck)[j].idsec == ENV)
					count++ ;
				
				if (genalloc_s(keynocheck,nocheck)[j].idsec == MAIN) 
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
					VERBO3 strerr_warnw1x("options env was asked -- section environment must be set") ;
					return 0 ;
				}
			}
			break ;
		default: break ;
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
						return 0 ;
				
				break ;
			case EXECRUN:
				if (nocheck->idsec == 1)
					if (!keep_runfinish(&service->type.classic_longrun.run,nocheck))
						return 0 ;
					 
				break ;
			case EXECFINISH:
				if (nocheck->idsec == 2)
					if (!keep_runfinish(&service->type.classic_longrun.finish,nocheck))
						return 0 ;
				 
				break ;
			case EXECLOG:
				if (nocheck->idsec == 3)
					if (!keep_logger(&service->type.classic_longrun.log,nocheck))
						return 0 ;
				 
				break ;
			case EXECUP:
				if (nocheck->idsec == 1)
					if (!keep_runfinish(&service->type.oneshot.up,nocheck))
						return 0 ;
				 
				break ;
			case EXECDOWN:
				if (nocheck->idsec == 2)
					if (!keep_runfinish(&service->type.oneshot.down,nocheck))
						return 0 ;
				 
				break ;
			case ENVIRON:
				if (nocheck->idsec == 4)
					if (!keep_common(service,nocheck,svtype))
						return 0 ;
				
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

/**********************************
 *		store
 * *******************************/

int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype)
{
	int r, nbline ;
	unsigned int i ;
	genalloc gatmp = GENALLOC_ZERO ;
	stralloc satmp = STRALLOC_ZERO ;
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
		case HIERCOPY:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			for (i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
			{
				char *name = gaistr(&gatmp,i) ;
				size_t namelen = gaistrlen(&gatmp,i) ;
				service->hiercopy[i+1] = keep.len ;
				if (!stralloc_catb(&keep,name,namelen + 1)) retstralloc(0,"parse_common:hiercopy") ;
				service->hiercopy[0] = i+1 ;
			}
			break ;
		case DEPENDS:
			if (service->cname.itype == CLASSIC)
			{
				VERBO3 strerr_warnw3x("key : ",get_keybyid(nocheck->idkey)," : is not valid for type classic") ;
				return 0 ;
			}
			else
			if (service->cname.itype == BUNDLE)
			{
				VERBO3 strerr_warnw3x("key : ",get_keybyid(nocheck->idkey)," : is not valid for type bundle") ;
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
			if (service->cname.itype != BUNDLE)
			{
				VERBO3 strerr_warnw4x("key : ",get_keybyid(nocheck->idkey)," : is not valid for type ",get_keybyid(service->cname.itype)) ;
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
				satmp.len = 0 ;
				if (!stralloc_cats(&satmp,gaistr(&gatmp,i)) ||
				!stralloc_0(&satmp)) 
				{
					VERBO3 strerr_warnwu2x("append environment value: ",gaistr(&gatmp,i)) ;
					stralloc_free(&satmp) ;
					return 0 ;
				}
				r = env_clean(&satmp) ;
				if (r > 0)
				{
					if (!stralloc_cats(&saenv,satmp.s) ||
					!stralloc_cats(&saenv,"\n"))
					{
						VERBO3 strerr_warnwu2x("store environment value: ",gaistr(&gatmp,i)) ;
						stralloc_free(&satmp) ;
						return 0 ;
					}
				}
				else if (!r)
				{
					VERBO3 strerr_warnwu2x("clean environment value: ",gaistr(&gatmp,i)) ;
					stralloc_free(&satmp) ;
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
			VERBO3 strerr_warnw2x("unknown key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	stralloc_free(&satmp) ;
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
			VERBO3 strerr_warnw2x("unknown key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
		}

	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
}

int keep_logger(sv_execlog *log,keynocheck *nocheck)
{
	int r, i ;

	genalloc gatmp = GENALLOC_ZERO ;

	switch(nocheck->idkey){
		case BUILD:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case RUNAS:
			if (!keep_runfinish(&log->run,nocheck)) return 0 ;
			break ;
		case DEPENDS:
			if (!clean_val(&gatmp,nocheck->val.s))
			{
				VERBO3 strerr_warnwu2x("parse file ",nocheck->val.s) ;
				return 0 ;
			}
			log->idga = genalloc_len(unsigned int,&gadeps) ;
			for (i = 0;i<genalloc_len(stralist,&gatmp);i++)
			{
				if (!genalloc_append(unsigned int,&gadeps,&deps.len)) retstralloc(0,"parse_logger") ;
				if (!stralloc_catb(&deps,gaistr(&gatmp,i),gaistrlen(&gatmp,i) + 1)) retstralloc(0,"parse_logger") ;
				log->nga++ ;
			}
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
			VERBO3 strerr_warnw2x("unknown key: ",get_keybyid(nocheck->idkey)) ;
			return 0 ;
	}
	
	genalloc_deepfree (stralist,&gatmp,stra_free) ;
	
	return 1 ;
}

/**********************************
 *		helper function
 * *******************************/
int read_svfile(stralloc *sasv,char const *name,char const *src)
{
	int r ; 
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char svtmp[srclen + 1 + namelen + 1] ;
	memcpy(svtmp,src,srclen) ;
	svtmp[srclen] = '/' ;
	memcpy(svtmp + srclen + 1, name, namelen) ;
	svtmp[srclen + 1 + namelen] = 0 ;
	
	VERBO3 strerr_warni4x("Read service file of : ",name," from: ",src) ;
	
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

ssize_t get_sep_before (char const *line, char const sepstart, char const sepend)
{
	size_t linend, linesep ;
	linesep=get_len_until(line,sepstart) ;
	linend=get_len_until(line,sepend) ;
	if (linesep > linend) return -1 ;
	if (!linend) return 0 ;
	return linesep ;
}

void section_setsa(int id, stralloc_ref *p,section_t *sa) 
{
	switch(id)
	{
		case MAIN: *p = &sa->main ; break ;
		case START: *p = &sa->start ; break ;
		case STOP: *p = &sa->stop ; break ;
		case LOG: *p = &sa->logger ; break ;
		case ENV: *p = &sa->environment ; break ;
		default: break ;
	}
}

int section_skip(char const *s,size_t pos,int nline)
{
	ssize_t r = -1 ;
	if (nline == 1) 
	{
		r = get_sep_before(s,'#','\n') ;
		if (r >= 0) return 0 ;
	}
	r = get_rlen_until(s,'\n',pos) ;
	if (r >= 0)
	{
		r = get_sep_before(s+r+1,'#','\n') ;
		if (r >= 0) return 0 ;
	}
	return 1 ;
}

/*@Return 1 on success
 * @Return 0 on fail
 * @Return -1 on invalid section */
int section_valid(int id, uint32_t nline, size_t pos,stralloc *src, char const *file)
{
	int r, rn, found = 0, err = 0 ;
	size_t tpos = 0 ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc fake = STRALLOC_ZERO ;
	key_all_t const *list = total_list ;
	parse_mill_t key = { .open = '@', .close = '=', \
							.skip = " \n\t\r", .skiplen = 4, \
							.end = 0, .endlen = 0, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 0, \
							.forceskip = 1, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	
	/* keys like execute can contain '[]' regex character,
	 * check it and ignore the regex if it's the case*/
	r = get_rlen_until(src->s,'\n',pos) ;
	/*we are on first line?*/
	if (r < 0 && nline > 1) goto err ;
	else if (nline == 1) goto freed ;
	r++;
	rn = get_sep_before(src->s+r,'@','\n') ;
	if (rn < 0) goto invalid ;
	if (!stralloc_cats(&tmp,src->s+r)) goto err ;
	if (!stralloc_0(&tmp)) goto err ;
	if (!parse_config(&key,file,&tmp,&fake,&tpos)) goto err ;
	if (!fake.len) goto err ;
	stralloc_inserts(&fake,0,"@") ;
	stralloc_0(&fake) ;
	for (int i = 0 ; i < key_enum_section_el; i++)
	{
		for (int j = 0 ; j < total_list_el[i]; j++)
		{
			if (list[i].list[j].name && obstr_equal(fake.s,list[i].list[j].name))
			{ found = 1 ; break ; }
		}
	}
	if (!found) goto invalid ;
	freed:
		stralloc_free(&tmp) ;
		stralloc_free(&fake) ;
		return 1 ;
	invalid:
		err = -1 ;
	err:
		stralloc_free(&tmp) ;
		stralloc_free(&fake) ;
		return err ;
}

int clean_value(stralloc *sa)
{
	size_t pos = 0 ;
	char const *file = "clean_value" ;
	stralloc tmp = STRALLOC_ZERO ;
	parse_mill_t empty = { .open = '@', .close = ' ', \
							.skip = " \n\t\r", .skiplen = 4, \
							.end = 0, .endlen = 0, \
							.jump = 0, .jumplen = 0,\
							.check = 0, .flush = 0, \
							.forceskip = 1, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	if (!stralloc_inserts(sa,0,"@")) goto err ;
	if (!stralloc_cats(sa," ")) goto err ;
	if (!parse_config(&empty,file,sa,&tmp,&pos)) goto err ;
	if (!stralloc_0(&tmp)) goto err ;
	if (!stralloc_copy(sa,&tmp)) goto err ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

void parse_err(int ierr,int idsec,int idkey)
{
	switch(ierr)
	{
		case 0: 
			strerr_warnw4x("invalid value for key: ",get_keybyid(idkey)," in section: ",get_keybyid(idsec)) ;
			break ;
		case 1:
			strerr_warnw4x("multiple definition of key: ",get_keybyid(idkey)," in section: ",get_keybyid(idsec)) ;
			break ;
		case 2:
			strerr_warnw4x("same value for key: ",get_keybyid(idkey)," in section: ",get_keybyid(idsec)) ;
			break ;
		case 3:
			strerr_warnw4x("key: ",get_keybyid(idkey)," must be an integrer value in section: ",get_keybyid(idsec)) ;
			break ;
		case 4:
			strerr_warnw4x("key: ",get_keybyid(idkey)," must be an absolute path in section: ",get_keybyid(idsec)) ;
			break ;
		case 5:
			strerr_warnw4x("key: ",get_keybyid(idkey)," must be set in section: ",get_keybyid(idsec)) ;
			break ;
		case 6:
			strerr_warnw4x("invalid format of key: ",get_keybyid(idkey)," in section: ",get_keybyid(idsec)) ;
			break ;
		default:
			strerr_warnw1x("unknown parse_err number") ;
			break ;
	}
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
