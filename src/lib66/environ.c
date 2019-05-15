/* 
 * environ.c
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
 
#include <stddef.h>
#include <stdio.h>

#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/diuint32.h>

#include <66/parser.h>

/* @Return 1 on success
 * @Return 0 on fail
 * @Return -1 for empty line */
int env_clean(stralloc *src)
{
	int r, e = 1 ;
	unsigned int i ;
	genalloc gatmp =GENALLOC_ZERO ;//stralist
	stralloc kp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
		
	size_t pos = 0 ;
	char const *file = "env_clean" ;
	parse_mill_t line = { .open = '@', .close = '=', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	parse_mill_t line_end = { .open = '@', .close = '\n', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	size_t blen = src->len, n = 0 ;
	if (!stralloc_inserts(src,0,"@")) goto err ;
	while(pos < (blen+n) && n < 2)
	{
		kp.len = 0 ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		line.inner.nopen = line.inner.nclose = 0 ;
		r = parse_config(!n?&line:&line_end,file,src,&kp,&pos) ;
		if (!r) goto err ;
		if (r < 0 && !n){ e = -1 ; goto freed ; }
		if (!stralloc_0(&kp)) goto err ;
		if (!clean_val(&gatmp,kp.s)) goto err ;
		for (i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
		{	
			if ((i+1) < genalloc_len(stralist,&gatmp))
			{
				if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
				if (!stralloc_cats(&tmp," ")) goto err ;
			}
			else if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
		}
		if (!n) if (!stralloc_cats(&tmp,"=")) goto err ;
		if (!stralloc_inserts(src,pos,"@")) goto err ;
		n++;
	}
	
	if (!stralloc_0(&tmp)) goto err ;
	if (!stralloc_copy(src,&tmp)) goto err ;
	
	freed:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return e ;
	
	err:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
}

int env_split_one(char *line,genalloc *ga,stralloc *sa)
{
	char *k = 0 ;
	char *v = 0 ;
	diuint32 tmp = DIUINT32_ZERO ;
	k = line ;
	v = line ;
	
	obstr_sep(&v,"=") ;
	if (v == NULL) return 0 ;
		
	tmp.left = sa->len ;
	if(!stralloc_catb(sa,k,strlen(k)+1)) return 0 ;
	
	if (!obstr_trim(v,'\n')) return 0 ;
	tmp.right = sa->len ;
	if(!stralloc_catb(sa,v,strlen(v)+1)) return 0 ;
	
	if (!genalloc_append(diuint32,ga,&tmp)) return 0 ;
		
	return 1 ;
}

int env_split(genalloc *gaenv,stralloc *saenv,stralloc *src)
{
	int nbline = 0, i = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
	stralloc tmp = STRALLOC_ZERO ;
	nbline = get_nbline_ga(src->s,src->len,&gatmp) ;
	for (; i < nbline ; i++)
	{
		char *line = gaistr(&gatmp,i) ;
		tmp.len = 0 ;
		stralloc_cats(&tmp,line) ;
		/** skip commented line or empty line*/
		if (env_clean(&tmp) < 0) continue ;
		if (*line)
			if (!env_split_one(line,gaenv,saenv)) goto err ;
	}
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err: 
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

int env_parsenclean(stralloc *modifs,stralloc *src)
{
	int nbline = 0, i = 0 ;
	size_t pos = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
	stralloc tmp = STRALLOC_ZERO ;
	nbline = get_nbline_ga(src->s,src->len,&gatmp) ;
	
	for (; i < nbline ; i++)
	{
		tmp.len = 0 ;
		if (!gaistrlen(&gatmp,i)) break ;
		if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
		if (!parse_env(&tmp,&pos)) goto err ;
		if (!env_clean(&tmp)) goto err ;
		tmp.len--;//remove '0'
		int u = 0 ;
		if (tmp.s[0] == '!') u++ ;
		if (!stralloc_catb(modifs,tmp.s + u ,(tmp.len - u) + 1)) goto err ;// ||
//		!stralloc_0(modifs)) goto err ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
