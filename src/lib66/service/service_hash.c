/*
 * service_hash.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>

int resolve_hash_add(hash_t *hash, char const *name, resolve_service_t res)
{
    log_flow() ;

	struct resolve_hash_s *s ;
	s = malloc(sizeof(*s));
	if (s == NULL)
		return 0 ;

	memset(s, 0, sizeof(*s)) ;

	s->limit = (resolve_service_addon_limit_t)RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
	s->environ = (resolve_service_addon_environ_t)RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
	s->io = (resolve_service_addon_io_t)RESOLVE_SERVICE_ADDON_IO_ZERO ;
	s->logger = (resolve_service_addon_logger_t)RESOLVE_SERVICE_ADDON_LOGGER_ZERO ;
	s->execute = (resolve_service_addon_execute_t)RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
	s->dependencies = (resolve_service_addon_dependencies_t)RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
	s->regex = (resolve_service_addon_regex_t)RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
	s->event = (resolve_service_addon_event_t)RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
	s->before = (resolve_service_t)RESOLVE_SERVICE_ZERO ;
	s->beforedeps = (resolve_service_addon_dependencies_t)RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
	auto_strings(s->name, name) ;
	s->res = res ;

	if (!hash_add(hash, s->name, strlen(s->name), s)) {
		free(s) ;
		return 0 ;
	}

	return 1 ;
}

struct resolve_hash_s *resolve_hash_search(hash_t *hash, char const *name)
{
    log_flow() ;

	return hash_find(hash, name, strlen(name)) ;
}

void resolve_hash_free(hash_t *hash)
{
    log_flow() ;

	struct resolve_hash_s *c, *tmp ;

	HASH_FOREACH(hash, c, tmp) {
		service_read_free(c) ;
		free(c) ;
	}

	hash_free(hash) ;
}
