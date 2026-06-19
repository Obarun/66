/*
 * tree_hash.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash2.h>

#include <66/tree.h>
#include <66/resolve.h>

int hash_add_tree(hash_t *hash, char const *name, resolve_tree_t res)
{
    log_flow() ;

	struct resolve_hash_tree_s *s ;
	s = malloc(sizeof(*s));
	if (s == NULL)
		return 0 ;

	memset(s, 0, sizeof(*s)) ;
	s->visit = 0 ;
	auto_strings(s->name, name) ;
	s->tres = res ;

	if (!hash_add(hash, s->name, strlen(s->name), s)) {
		free(s) ;
		return 0 ;
	}

	return 1 ;
}

struct resolve_hash_tree_s *hash_search_tree(hash_t *hash, char const *name)
{
    log_flow() ;

	return hash_find(hash, name, strlen(name)) ;
}

int hash_count_tree(hash_t *hash)
{
	return hash_count(hash) ;
}

void hash_free_tree(hash_t *hash)
{
    log_flow() ;

	struct resolve_hash_tree_s *c, *tmp ;

	HASH_FOREACH(hash, c, tmp) {
		strbuf_free(&c->tres.sa) ;
		free(c) ;
	}

	hash_free(hash) ;
}
