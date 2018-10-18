/* 
 * db_get_permissions.c
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

#include <66/utils.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/stralist.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>

#include <66/constants.h>


int db_get_permissions(stralloc *uid, char const *tree)
{
	genalloc list = GENALLOC_ZERO ;

	size_t treelen = strlen(tree) ;
	char tmp[treelen + SS_RULES_LEN + 1] ;
	memcpy(tmp,tree,treelen) ;
	memcpy(tmp + treelen, SS_RULES, SS_RULES_LEN) ;
	tmp[treelen + SS_RULES_LEN] = 0 ;
	
	if (!file_get_fromdir(&list,tmp)) return 0 ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&list); i++)
	{
		if (!stralloc_cats(uid,gaistr(&list,i))) retstralloc(0,"set_permissions_db") ;
		if (!stralloc_cats(uid,",")) retstralloc(0,"set_permissions_db") ;	
	}
	uid->len-- ;// remove the last ','
	if (!stralloc_0(uid)) retstralloc(0,"set_permissions_db") ;		
	
	genalloc_deepfree(stralist,&list,stra_free) ;
	
	return 1 ;
}
