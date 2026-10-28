/*
 * service_read.c
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
#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/config.h>

typedef struct addon_s addon_t ;
struct addon_s
{
    uint32_t bit ;
    uint32_t declared ;
    uint8_t type ;
    void *obj ;
} ;

static int read_resolve(struct resolve_hash_s *h, char const *base, char const *name)
{
    size_t namelen = strlen(name) ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &h->res) ;

    int r = resolve_read(&w, base, name) ;

    if (r <= 0) {
        resolve_free_obj(&w) ;
        h->res = (resolve_service_t)RESOLVE_SERVICE_ZERO ;
        return r ;
    }

    memcpy(h->name, name, namelen + 1) ;

    return 1 ;
}

static int read_addon(addon_t *a, struct resolve_hash_s *h, char const *base)
{
    resolve_wrapper_t w = RESOLVE_WRAPPER(a->type, a->obj) ;
    int r = resolve_read(&w, base, h->name) ;

    if (r <= 0)
        resolve_free_obj(&w) ;

    // a declared addon that is missing is a resolve file that does not read
    return r ? r : (errno = EINVAL, 0) ;
}

int service_read(struct resolve_hash_s *h, char const *base, char const *name, uint32_t addons)
{
    log_flow() ;

    int r ;

    if (addons & ~SERVICE_READ_ALL)
        return (errno = EINVAL, -1) ;

    if (!h->name[0]) {

        r = read_resolve(h, base, name) ;
        if (r <= 0)
            return r ;

    } else if (strcmp(h->name, name))
        return (errno = EINVAL, -1) ;

    addon_t list[] = {
        { SERVICE_READ_DEPENDENCIES, h->res.has_dependencies, DATA_SERVICE_DEPENDENCIES, &h->dependencies },
        { SERVICE_READ_EVENT, h->res.has_event, DATA_SERVICE_EVENT, &h->event },
        { SERVICE_READ_EXECUTE, h->res.has_execute, DATA_SERVICE_EXECUTE, &h->execute },
        { SERVICE_READ_IO, h->res.has_io, DATA_SERVICE_IO, &h->io },
        { SERVICE_READ_ENVIRON, h->res.has_environ, DATA_SERVICE_ENVIRON, &h->environ },
        { SERVICE_READ_LOGGER, 0, DATA_SERVICE_LOGGER, &h->logger },
        { SERVICE_READ_LIMIT, h->res.has_limit, DATA_SERVICE_LIMIT, &h->limit },
        { SERVICE_READ_REGEX, h->res.has_regex, DATA_SERVICE_REGEX, &h->regex },
    } ;

    for (size_t i = 0 ; i < sizeof(list) / sizeof(list[0]) ; i++) {

        addon_t *a = &list[i] ;

        if (!(addons & a->bit) || (h->read & a->bit))
            continue ;

        if (a->declared) {

            r = read_addon(a, h, base) ;
            if (r <= 0)
                return r ;
        }

        h->read |= a->bit ;
    }

    return 1 ;
}
