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
 
#include <string.h>
//#include <stdio.h>

#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>
#include <oblibs/obgetopt.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/parser.h>

inline uint8_t cclass (parse_mill_t *p)
{
	size_t i = 0 ;
	
	if (!p->inner.curr) return 0 ;
	else if (p->inner.curr == '\n')
	{ 
		if (p->flush) p->inner.flushed = 1 ;
		p->inner.jumped = 0 ; 
		p->inner.nline++ ;
	}
	for (; i < p->jumplen ; i++)
	{
		if (p->inner.curr == p->jump[i])
		{
			p->inner.jumped = 1 ;
			return 2 ;
		}
	}
	for (i = 0 ; i < p->endlen ; i++)
	{
		if (p->inner.curr == p->end[i])
		{
			if (p->inner.curr == p->close) p->inner.nclose++ ;
			return 3 ;
		}
	}
	for (i = 0 ; i < p->skiplen ; i++)
	{
		if (p->inner.curr == p->skip[i])
		{
			if (p->open && !p->forceskip) return 1 ;
			return 0 ;
		}
	}
	/* close and open can be the same, in this case
	 * we skip open if it already found */
	if (p->inner.curr == p->open && !p->inner.nopen)
	{
		p->inner.nopen++ ;
		return 0 ;
	}
	
	if (p->inner.curr == p->close)
	{
		p->inner.nclose++ ;
		if (p->force) return 3 ;
		return 0 ;
	}

	return 1 ;
}

inline char next(stralloc *s,size_t *pos)
{
	char c ;
	if (*pos >= s->len) return -1 ;
	c = s->s[*pos] ;
	(*pos) += 1 ;
	return c ;
}
/** @Return 1 on sucess
 * @Return 0 on fail
 * @Return 2 for end of file
 * @Return -1 if close was not found */  
inline int parse_config(parse_mill_t *p,char const *file, stralloc *src, stralloc *kp,size_t *pos)
{
	uint8_t what = 0 ;
	static uint8_t const table[5] = { IGN, KEEP, JUMP, EXIT, END } ;
	uint8_t state = 1, end = 0 ;
	char j = 0 ;
	while (state)
	{
		p->inner.curr = next(src, pos) ;
		what = table[cclass(p)] ;
		// end of file	
		if (p->inner.curr == -1) what = END ;
		if (p->inner.flushed)
		{
			kp->len = 0 ;
			p->inner.nopen = 0 ;
			p->inner.nclose = 0 ;
			p->inner.flushed = 0 ;
			what = SKIP ;
		}
		switch(what)
		{
			case KEEP:
				if (p->inner.nopen && !p->inner.jumped)
					if (!stralloc_catb(kp,&p->inner.curr,1)) return 0 ; 
				break ;
			case JUMP:
				if (!p->inner.nopen)
				{
					while (j != '\n')
					{
						j = next(src,pos) ;
						if (j < 0) break ;//end of string
					}
					p->inner.jumped = 0 ;
					p->inner.nline++ ;
				}
				break ;
			case IGN:
				break ;
			case EXIT:
				state = 0 ;
				break ;
			case END:
				state = 0 ;
				end = 1 ;
				break ;
			default: break ;
		}
	}
		
	if (p->check && p->inner.nopen != p->inner.nclose)
	{
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, p->inner.nline-1)] = 0 ;
		char sepopen[2] = { p->open,0 } ;
		char sepclose[2] = { p->close,0 } ;
		strerr_warnw6x("umatched ",(p->inner.nopen > p->inner.nclose) ? sepopen : sepclose," in: ",file," at line: ",fmt) ;
		return 0 ;
	}
	if (!p->inner.nclose) return -1 ;
	if (end) return 2 ;
	return 1 ;
}

int parser(sv_alltype *service,stralloc *src,char const *file)
{
	int r ;
	int svtype = -1 ;
	section_t sasection = SECTION_ZERO ;
	genalloc ganocheck = GENALLOC_ZERO ;
	sasection.file = file ;
	
	r = get_section_range(&sasection,src) ;
	if (r <= 0){
		strerr_warnwu2x("parse section of service file: ",file) ;
		goto err ;
	}
	if (!sasection.idx[MAIN])
	{
		VERBO1 strerr_warnw2x("missing section [main] in service file: ", file) ;
		goto err ;
	}
	if (!get_key_range(&ganocheck,&sasection,file,&svtype)) goto err ;
	if (svtype < 0)
	{
		VERBO1 strerr_warnw2x("invalid value for key: @type in service file: ",file) ;
		goto err ;
	}
	if (svtype != BUNDLE && !sasection.idx[START])
	{
		VERBO1 strerr_warnw2x("missing section [start] in service file: ", file) ;
		goto err ;
	}
	if (!genalloc_len(keynocheck,&ganocheck)){
		VERBO1 strerr_warnw2x("empty service file: ",file) ;
		goto err ;
	}
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&ganocheck);i++)
	{
		uint32_t idsec = genalloc_s(keynocheck,&ganocheck)[i].idsec ;
		for (int j = 0;j < total_list_el[idsec] && total_list[idsec].list > 0;j++)
		{
			if (!get_mandatory(&ganocheck,idsec,j))
			{
				VERBO1 strerr_warnw2x("mandatory key is missing in service file: ",file) ; 
				goto err ;
			}
		}
	}
	for (unsigned int i = 0;i < genalloc_len(keynocheck,&ganocheck);i++)
	{
		if (!nocheck_toservice(&(genalloc_s(keynocheck,&ganocheck)[i]),svtype,service))
		{ 
			VERBO1 strerr_warnwu2x("keep information of service file: ",file) ;
			goto err ;
		}
	}
	if ((service->opts[1]) && (svtype == LONGRUN))
	{
		if (!add_pipe(service, &deps))
		{
			VERBO1 strerr_warnwu2x("add pipe: ", keep.s+service->cname.name) ;
			goto err ;
		} 
	}
	
	section_free(&sasection) ;
	genalloc_deepfree(keynocheck,&ganocheck,keynocheck_free) ;
	return 1 ;
	err:
		section_free(&sasection) ;
		genalloc_deepfree(keynocheck,&ganocheck,keynocheck_free) ;
		return 0 ;
}


