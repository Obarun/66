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
#include <stdint.h>
#include <stdint.h>
//#include <stdio.h>

#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>
#include <oblibs/obgetopt.h>
#include <oblibs/mill.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/parser.h>


int parser(sv_alltype *service,stralloc *src,char const *svname)
{
	int r , svtype = -1 ;
	size_t i = 0 ;
	section_t sasection = SECTION_ZERO ;
	genalloc ganocheck = GENALLOC_ZERO ;
	sasection.file = svname ;
	
	r = section_get_range(&sasection,src) ;
	if (r <= 0){
		strerr_warnwu2x("parse section of service file: ",svname) ;
		goto err ;
	}
	if (!sasection.idx[MAIN])
	{
		VERBO1 strerr_warnw2x("missing section [main] in service file: ", svname) ;
		goto err ;
	}
	if (!key_get_range(&ganocheck,&sasection,&svtype)) goto err ;
	if (svtype < 0)
	{
		VERBO1 strerr_warnw4x("invalid value for key: ",get_keybyid(TYPE)," in service file: ",svname) ;
		goto err ;
	}
	if (svtype != BUNDLE && !sasection.idx[START])
	{
		VERBO1 strerr_warnw2x("missing section [start] in service file: ", svname) ;
		goto err ;
	}
	if (!genalloc_len(keynocheck,&ganocheck)){
		VERBO1 strerr_warnw2x("empty service file: ",svname) ;
		goto err ;
	}
	for (i = 0;i < genalloc_len(keynocheck,&ganocheck);i++)
	{
		uint32_t idsec = genalloc_s(keynocheck,&ganocheck)[i].idsec ;
		for (unsigned int j = 0;j < total_list_el[idsec] && total_list[idsec].list > 0;j++)
		{
			if (!get_mandatory(&ganocheck,idsec,j))
			{
				VERBO1 strerr_warnw2x("mandatory key is missing in service file: ",svname) ; 
				goto err ;
			}
		}
	}
	for (i = 0;i < genalloc_len(keynocheck,&ganocheck);i++)
	{
		if (!nocheck_toservice(&(genalloc_s(keynocheck,&ganocheck)[i]),svtype,service))
		{ 
			VERBO1 strerr_warnwu2x("keep information of service file: ",svname) ;
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


