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

#include <oblibs/hash.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>

struct resolve_hash_s *parse_get_hashname(char *store, hash_t *hres, char const *name, char const *ns)
{
    struct resolve_hash_s *hash = resolve_hash_search(hres, name) ;

    if (hash != NULL) {
        auto_strings(store, name) ;
        return hash ;
    }

    if (!ns) {
        auto_strings(store, name) ;
        return 0 ;
    }

    auto_strings(store, ns, ":", name) ;

    return resolve_hash_search(hres, store) ;
}
