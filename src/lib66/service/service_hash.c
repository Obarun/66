/*
 * service_hash.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/hash.h>

#include <66/resolve.h>

int hash_add(struct resolve_hash_s **hash, char const *name, resolve_service_t res)
{
    log_flow() ;

	struct resolve_hash_s *s ;
	s = (struct resolve_hash_s *)malloc(sizeof(*s));
	if (s == NULL)
		return 0 ;

	memset(s, 0, sizeof(*s)) ;
	s->visit = 0 ;
	auto_strings(s->name, name) ;
	s->res = res ;
	HASH_ADD_STR(*hash, name, s) ;

	return 1 ;
}

struct resolve_hash_s *hash_search(struct resolve_hash_s **hash, char const *name)
{
    log_flow() ;

	struct resolve_hash_s *s ;
	HASH_FIND_STR(*hash, name, s) ;
	return s ;

}

int hash_count(struct resolve_hash_s **hash)
{
	return HASH_COUNT(*hash) ;
}

void hash_free(struct resolve_hash_s **hash)
{
    log_flow() ;

	struct resolve_hash_s *c, *tmp ;

	HASH_ITER(hh, *hash, c, tmp) {
		stralloc_free(&c->res.sa) ;
		HASH_DEL(*hash, c) ;
		free(c) ;
	}
}
