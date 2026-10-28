/*
 * parse_get_hashname.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>

#include <oblibs/hash.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/config.h>

struct resolve_hash_s *parse_get_hashname(char *store, hash_t *hres, char const *name, char const *ns)
{
    size_t len = strlen(name) ;

    if (len > SS_MAX_SERVICE_NAME) {
        store[0] = 0 ;
        return 0 ;
    }

    struct resolve_hash_s *hash = resolve_hash_search(hres, name) ;

    auto_strings(store, name) ;

    if (hash != NULL || !ns || strlen(ns) + 1 + len > SS_MAX_SERVICE_NAME)
        return hash ;

    auto_strings(store, ns, ":", name) ;

    return resolve_hash_search(hres, store) ;
}
