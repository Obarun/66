/*
 * parse_check.c
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
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>
#include <oblibs/types.h>

#include <66/config.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/module.h>
#include <66/utils.h>
#include <66/symlink.h>

static char const *provides(struct resolve_hash_s const *c)
{
    return c->dependencies.nprovide ? c->dependencies.sa.s + c->dependencies.provide : 0 ;
}

static int check_closed(struct resolve_hash_s *c, strbuf *msg)
{
    char const *inns = c->res.inns ? c->res.sa.s + c->res.inns : 0 ;
    resolve_service_addon_dependencies_t const *dep = &c->dependencies ;
    char const *list[3] = {
        dep->ndepends ? dep->sa.s + dep->depends : 0,
        dep->nrequiredby ? dep->sa.s + dep->requiredby : 0,
        // the From of a member reactor: the members of its module only
        inns && c->res.has_event && c->event.nfrom ? c->event.sa.s + c->event.from : 0
    } ;

    char const *key[3] = { "depends on", "declares in its RequiredBy", "watches in its From" } ;
    char const *rule[3] = { "a member depends on", "a member declares in its RequiredBy", "a member watches" } ;

    for (uint8_t i = 0 ; i < 3 ; i++) {

        size_t pos = 0 ;
        _cleanup_sbl_ strbuf sa = SBL_ZERO ;

        if (!list[i])
            continue ;

        if (!sbl_clean_string(&sa, list[i]))
            return -1 ;

        FOREACH_SBL(&sa, pos) {

            char const *name = sa.s + pos ;
            char module[strlen(name) + 1] ;
            int member = module_getname(module, name) ;

            if (inns ? member && !strcmp(module, inns) : !member)
                continue ;

            // a member names a service outside its module
            if (inns)
                return auto_strbuf(msg, "service: ", c->name, ", a member of the module: ", inns, ", ", key[i], ": ", name, " outside its module -- ", rule[i], " the members of its module only") ? 0 : -1 ;

            // a service outside a module names one of its members: Depends or RequiredBy
            return auto_strbuf(msg, "service: ", c->name, " ", key[i], ": ", name, ", a member of the module: ", module, " -- ", i ? "declare the module: " : "depend on the module: ", module, " instead") ? 0 : -1 ;
        }
    }

    return 1 ;
}

static int check_provided(struct resolve_hash_s *c, hash_t *hres, service_graph_t const *g, char const *base, strbuf *msg)
{
    int r ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;

    if (!provides(c))
        return 1 ;

    if (!sbl_clean_string(&stk, provides(c)))
        return -1 ;

    FOREACH_SBL(&stk, pos) {

        char const *name = stk.s + pos ;
        struct resolve_hash_s *other = 0 ;

        if (resolve_hash_search(hres, name))
            return auto_strbuf(msg, "service: ", c->name, " provides the name: ", name, ", parsed along with it -- drop one of the two frontends") ? 0 : -1 ;

        r = service_resolve_provide_along(&other, hres, c, name) ;
        if (r < 0)
            return -1 ;

        if (r)
            return auto_strbuf(msg, "service: ", c->name, " provides the name: ", name, ", provided as well by: ", other->name, ", parsed along with it -- drop the name from one of the two frontends") ? 0 : -1 ;

        r = symlink_provide_claim(g, base, hres, c->name, name, msg) ;
        if (r <= 0)
            return r ;
    }

    return 1 ;
}

static int depends_of(strbuf *sa, service_graph_t const *g, uint32_t index, hash_t *hres, char const *base)
{
    int r ;
    char const *name = service_graph_name(g, index) ;
    struct resolve_hash_s *c = resolve_hash_search(hres, name) ;
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;

    if (c) {
        if (c->dependencies.ndepends && !auto_strbuf(sa, c->dependencies.sa.s + c->dependencies.depends))
            return -1 ;
        return 1 ;
    }

    r = service_read(&h, base, name, SERVICE_READ_DEPENDENCIES) ;

    if (r > 0 && h.dependencies.ndepends && !auto_strbuf(sa, h.dependencies.sa.s + h.dependencies.depends))
        r = -1 ;

    service_read_free(&h) ;

    return r ;
}

static int read_before(struct resolve_hash_s *c, char const *base)
{
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    int r = service_read(&h, base, c->name, SERVICE_READ_DEPENDENCIES) ;

    if (r < 0) {
        service_read_free(&h) ;
        return -1 ;
    }

    strbuf_free(&c->before.sa) ;
    strbuf_free(&c->beforedeps.sa) ;

    // moved to @c, not freed with @h; a core that does not read leaves h.res zero
    c->before = h.res ;
    h.res = (resolve_service_t)RESOLVE_SERVICE_ZERO ;
    c->beforedeps = (resolve_service_addon_dependencies_t)RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;

    if (r) {
        c->beforedeps = h.dependencies ;
        h.dependencies = (resolve_service_addon_dependencies_t)RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    }

    c->beforeread = 1 ;
    service_read_free(&h) ;

    return 1 ;
}

static int check_released(struct resolve_hash_s *c, hash_t *hres, service_graph_t const *g, char const *base, strbuf *msg)
{
    int r ;
    size_t pos = 0 ;
    uint32_t index = service_graph_find(g, c->name) ;
    resolve_service_addon_dependencies_t const *old = &c->beforedeps ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;

    // the names it answered to as it was parsed, read by read_before(): never parsed, or unreadable, it provides nothing
    if (old->nprovide && !sbl_clean_string(&names, old->sa.s + old->provide))
        return -1 ;

    FOREACH_SBL(&names, pos) {

        char const *name = names.s + pos ;
        char holder[SS_MAX_SERVICE_NAME + 1] ;

        r = provides(c) ? name_inlist(provides(c), name) : 0 ;
        if (r < 0)
            return -1 ;

        if (r)
            continue ;

        r = service_resolve_provide(holder, name, 0, base) ;
        if (r < 0)
            return -1 ;

        // nobody, or another provider holds the name: its consumers do not reach @c
        if (!r || strcmp(holder, c->name))
            continue ;

        uint32_t const *rby = service_graph_requiredby(g, index) ;
        uint8_t const *origin = service_graph_rorigin(g, index) ;

        for (uint32_t n = 0 ; n < service_graph_vertex(g, index)->nrequiredby ; n++) {

            _cleanup_strbuf_ strbuf deps = STRBUF_ZERO ;

            if (!(origin[n] & SERVICE_GRAPH_EDGE_DECLARED))
                continue ;

            r = depends_of(&deps, g, rby[n], hres, base) ;
            if (r < 0)
                return -1 ;

            if (!r || !deps.len)
                continue ;

            r = name_inlist(deps.s, name) ;
            if (r < 0)
                return -1 ;

            if (r) {
                char const *consumer = service_graph_name(g, rby[n]) ;
                return auto_strbuf(msg, "service: ", c->name, " no longer provides the name: ", name, ", which service: ", consumer, " depends on -- remove it from the Depends of: ", consumer, " first") ? 0 : -1 ;
            }
        }
    }

    return 1 ;
}

static int on_cycle(service_graph_t const *g, uint32_t index, uint32_t *parent, uint8_t *seen, uint32_t *stack)
{
    uint32_t n = service_graph_nvertex(g), top = 0 ;

    memset(seen, 0, n) ;

    stack[top++] = index ;
    parent[index] = SERVICE_GRAPH_NOINDEX ;

    while (top) {

        uint32_t v = stack[--top] ;
        uint32_t const *dep = service_graph_depends(g, v) ;

        for (uint32_t pos = 0 ; pos < service_graph_vertex(g, v)->ndepends ; pos++) {

            uint32_t w = dep[pos] ;

            if (w == index) {
                parent[index] = v ;
                return 1 ;
            }

            if (seen[w])
                continue ;

            seen[w] = 1 ;
            parent[w] = v ;
            stack[top++] = w ;
        }
    }

    return 0 ;
}

static int cycle_message(strbuf *msg, service_graph_t const *g, uint32_t index, uint32_t const *parent)
{
    uint32_t n = service_graph_nvertex(g), len = 0, path[n + 1] ;

    // the parents lead backward from the vertex that closes the cycle to @index
    for (uint32_t v = parent[index] ; v != index && len < n ; v = parent[v])
        path[len++] = v ;

    if (!auto_strbuf(msg, "cyclic dependencies detected -- ", service_graph_name(g, index)))
        return -1 ;

    while (len)
        if (!auto_strbuf(msg, " -> ", service_graph_name(g, path[--len])))
            return -1 ;

    return auto_strbuf(msg, " -> ", service_graph_name(g, index)) ? 0 : -1 ;
}

int parse_check(hash_t *hres, ssexec_t *info, strbuf *msg)
{
    log_flow() ;

    int r, e = -1 ;
    char const *base = info->base.s ;
    struct resolve_hash_s *c, *tmp ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;

    if (service_graph_build(&g, base, hres) < 0) {

        if (errno != EOVERFLOW)
            return -1 ;

        char fmt[U32_FMT] ;
        fmt[u32_fmt(fmt, SS_MAX_SERVICE)] = 0 ;

        return auto_strbuf(msg, "too many services: the system holds at most: ", fmt, " -- build 66 again with a larger -Dmax-service") ? 0 : -1 ;
    }

    {
        uint32_t n = service_graph_nvertex(&g) ;
        uint32_t parent[n + 1], stack[n + 1] ;
        uint8_t seen[n + 1] ;

        // only a cycle a service of @hres is part of is refused: one it merely depends on lies on the disk already, outside the command
        HASH_FOREACH(hres, c, tmp) {

            uint32_t index = service_graph_find(&g, c->name) ;

            if (service_graph_state(&g, index) != SERVICE_GRAPH_STATE_UNORDERED || !on_cycle(&g, index, parent, seen, stack))
                continue ;

            e = cycle_message(msg, &g, index, parent) ;
            goto freed ;
        }
    }

    HASH_FOREACH(hres, c, tmp) {

        if (c->res.islog)
            continue ;

        // what @c was before the command, kept by parse_write() and read by check_released()
        if ((r = read_before(c, base)) <= 0 ||
            (r = check_closed(c, msg)) <= 0 ||
            (r = check_provided(c, hres, &g, base, msg)) <= 0 ||
            (r = check_released(c, hres, &g, base, msg)) <= 0) {
                e = r ;
                goto freed ;
        }
    }

    e = 1 ;

    freed:
        service_graph_free(&g) ;
        return e ;
}
