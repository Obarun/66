/*
 * parse_names.c
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
#include <stdbool.h>
#include <unistd.h> // getuid

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/parse.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/module.h>
#include <66/utils.h>
#include <66/symlink.h>

static int unknown(strbuf *msg, char const *name)
{
    return auto_strbuf(msg, SSEXEC_MSG_UNKNOWN(name)) ? 0 : -1 ;
}

static int check_operand(strbuf *todo, strbuf *walk, char const *arg, service_graph_t const *g, ssexec_t *info, uint32_t flags, strbuf *msg)
{
    int r ;
    size_t len = strlen(arg) ;
    char bname[len + 1], dname[len + 1] ;
    char designated[SS_MAX_SERVICE_NAME + 1] ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;
    char const *name = arg, *forced = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (arg[0] == '/') {

        if (!ob_dirname(dname, arg) || !ob_basename(bname, arg))
            return -1 ;

        name = bname ;
        forced = dname ;
    }

    if (!name[0] || strlen(name) > SS_MAX_SERVICE_NAME)
        return unknown(msg, name) ;

    holder[0] = 0 ;
    auto_strings(designated, name) ;

    if (!forced) {

        r = service_resolve_provide(holder, name, 0, info->base.s) ;
        if (r < 0)
            return -1 ;

        if (!r)
            holder[0] = 0 ;

        if (holder[0] && !(flags & PARSE_FRONTEND))
            auto_strings(designated, holder) ;
    }

    uint32_t index = service_graph_find_parsed(g, designated) ;

    if (index != SERVICE_GRAPH_NOINDEX && ((flags & PARSE_MISSING) || !(flags & PARSE_FORCE))) {

        if (!(flags & PARSE_MISSING))
            log_warn("ignoring service: ", designated, " -- already parsed") ;

        return sbl_add(walk, designated) ? 1 : -1 ;
    }

    r = name_isvalid(designated, msg) ;
    if (r <= 0)
        return r ;

    char module[strlen(designated) + 1] ;
    if (module_getname(module, designated))
        return auto_strbuf(msg, SSEXEC_MSG_MEMBER(designated, module, "parse")) ? 0 : -1 ;

    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    r = service_frontend_path(&sa, designated, info->owner, forced, exclude, 2) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        if (index != SERVICE_GRAPH_NOINDEX) {
            r = service_graph_invalid_message(msg, g, index, true) ;
            return r < 0 ? -1 : 0 ;
        }

        return unknown(msg, name) ;
    }

    if (holder[0] && (flags & PARSE_FRONTEND)) {

        r = symlink_provide_isfree(info->base.s, name, name, holder, msg) ;
        if (r <= 0)
            return r ;
    }

    return strbuf_catb(todo, sa.s, sa.len) ? 1 : -1 ;
}

static int parse_one(char const *sv, hash_t *hres, service_graph_t const *g, ssexec_t *info, uint32_t flags, strbuf *msg)
{
    parse_build_ctx_t ctx = {
        .hres = hres,
        .info = info,
        .conf = flags & PARSE_CONF ? 1 : 0,
        .flags = flags,
        .g = g,
        .msg = msg,
    } ;

    return parse_frontend(sv, ctx) ;
}

static int push_list(strbuf *queue, char const *list)
{
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;

    if (!*list)
        return 1 ;

    if (!sbl_clean_string(&stk, list))
        return -1 ;

    return strbuf_catb(queue, stk.s, stk.len) ? 1 : -1 ;
}

static int push_needs(strbuf *queue, char const *name, hash_t *hres, service_graph_t const *g, uint32_t flags)
{
    struct resolve_hash_s *c = resolve_hash_search(hres, name) ;

    if (c) {

        if (c->dependencies.ndepends && push_list(queue, c->dependencies.sa.s + c->dependencies.depends) < 0)
            return -1 ;

        if (!(flags & PARSE_NOFROM) && c->res.has_event && c->event.nfrom && push_list(queue, c->event.sa.s + c->event.from) < 0)
            return -1 ;

        return 1 ;
    }

    uint32_t index = service_graph_find_parsed(g, name) ;
    if (index == SERVICE_GRAPH_NOINDEX || !(flags & PARSE_MISSING))
        return 1 ;

    uint32_t const *dep = service_graph_depends(g, index) ;
    uint8_t const *origin = service_graph_dorigin(g, index) ;

    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, index)->ndepends ; pos++) {

        if ((origin[pos] & ~SERVICE_GRAPH_EDGE_LOGGER) == SERVICE_GRAPH_EDGE_FROM && (flags & PARSE_NOFROM))
            continue ;

        if (!sbl_add(queue, service_graph_name(g, dep[pos])))
            return -1 ;
    }

    return 1 ;
}

static int propagate_names(hash_t *hres, strbuf *walk, service_graph_t const *g, ssexec_t *info, uint32_t flags, strbuf *msg)
{
    int r ;
    size_t pos = 0 ;
    struct resolve_hash_s *c, *tmp ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    _cleanup_strbuf_ strbuf seen = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf queue = STRBUF_ZERO ;

    if (walk->len && !strbuf_catb(&queue, walk->s, walk->len))
        return -1 ;

    HASH_FOREACH(hres, c, tmp)
        if (!sbl_add(&queue, c->name))
            return -1 ;

    /** the queue grows while it is walked: FOREACH_SBL reads its length and its
     * buffer again at each turn */
    FOREACH_SBL(&queue, pos) {

        char held[SS_MAX_SERVICE_NAME + 1] ;
        char const *designated = queue.s + pos ;
        _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

        if (strlen(designated) > SS_MAX_SERVICE_NAME)
            continue ;

        if (service_graph_find_parsed(g, designated) == SERVICE_GRAPH_NOINDEX) {

            r = service_resolve_provide(held, designated, hres, info->base.s) ;
            if (r < 0)
                return -1 ;

            if (r)
                designated = held ;
        }

        if (sbl_search(&seen, designated) >= 0)
            continue ;

        if (!sbl_add(&seen, designated))
            return -1 ;

        if (resolve_hash_search(hres, designated) || service_graph_find_parsed(g, designated) != SERVICE_GRAPH_NOINDEX) {

            if (push_needs(&queue, designated, hres, g, flags) < 0)
                return -1 ;

            continue ;
        }

        char module[strlen(designated) + 1] ;
        if (module_getname(module, designated))
            continue ;

        r = service_frontend_path(&sa, designated, getuid(), 0, exclude, 2) ;
        if (r < 0)
            return -1 ;

        if (!r) {

            if (flags & PARSE_LEAVEGONE)
                continue ;

            return auto_strbuf(msg, "service: ", designated, " has neither a resolve file nor a frontend -- remove it with 66 remove ", designated) ? 0 : -1 ;
        }

        size_t p = 0 ;
        FOREACH_SBL(&sa, p) {

            r = parse_one(sa.s + p, hres, g, info, flags, msg) ;
            if (r <= 0)
                return r ;
        }

        if (push_needs(&queue, designated, hres, g, flags) < 0)
            return -1 ;
    }

    return 1 ;
}

static int none_absent(strbuf *walk, service_graph_t const *g, uint32_t flags)
{
    size_t pos = 0 ;
    bitset32_t named = bitset32_init(SS_MAX_SERVICE) ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = SELECTION_SERVICE_POLICY_ZERO ;

    policy.propagate = SELECTION_SERVICE_PROPAGATE_DEPENDS ;
    policy.from = !(flags & PARSE_NOFROM) ;

    FOREACH_SBL(walk, pos) {

        uint32_t index = service_graph_find_parsed(g, walk->s + pos) ;
        if (index == SERVICE_GRAPH_NOINDEX)
            return 0 ;

        bitset32_set(&named, index) ;
    }

    selection_service_propagate(&sel, g, &named, &policy) ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(g) ; index++)
        if (bitset32_isvalid(&sel.met, index))
            return 0 ;

    return 1 ;
}

int parse_names(hash_t *hres, service_graph_t const *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flags, strbuf *msg)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf todo = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf walk = STRBUF_ZERO ;

    for (int n = 0 ; n < argc ; n++) {

        r = check_operand(&todo, &walk, argv[n], g, info, flags, msg) ;
        if (r <= 0)
            return r ;
    }

    FOREACH_SBL(&todo, pos) {

        r = parse_one(todo.s + pos, hres, g, info, flags, msg) ;
        if (r <= 0)
            return r ;
    }

    if (flags & PARSE_NODEPS)
        return 1 ;

    if ((flags & PARSE_MISSING) && !hash_count(hres) && none_absent(&walk, g, flags))
        return 1 ;

    return propagate_names(hres, &walk, g, info, flags, msg) ;
}
