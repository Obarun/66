/*
 * symlink_provide.c
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
#include <stdbool.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>
#include <oblibs/directory.h>

#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/symlink.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/module.h>
#include <66/utils.h>

#define PROVIDE_DIR_LEN(base) (strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN)

int symlink_provide_drop(char const *base, char const *name)
{
    log_flow() ;

    size_t dlen = PROVIDE_DIR_LEN(base) ;
    char lnk[dlen + 1 + strlen(name) + 1] ;

    auto_strings(lnk, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;

    log_trace("name: ", name, " is answered by nobody") ;
    return dir_destroy(lnk) ? 1 : -1 ;
}

static int provide_move(char const *base, char const *name, char const *to)
{
    size_t dlen = PROVIDE_DIR_LEN(base), nlen = strlen(name) ;
    char lnk[dlen + 1 + nlen + 1] ;

    auto_strings(lnk, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;

    if (!dir_create_parent(lnk, 0755))
        return -1 ;

    auto_strings(lnk + dlen, "/", name) ;

    size_t tlen = strlen(to) ;
    char target[3 + tlen + 1] ;

    auto_strings(target, "../", to) ;

    char tmpdir[TMPDIR_GETLEN(strlen(base)) + 1] ;

    auto_strings(tmpdir, base, SS_TMPDIR + 1) ;

    log_trace("name: ", name, " is answered by: ", to) ;
    return symlink_atomic_at(tmpdir, target, lnk) ;
}

static int provide_read(strbuf *names, strbuf *holders, char const *base)
{
    size_t pos = 0 ;
    char dir[PROVIDE_DIR_LEN(base) + 1] ;
    char const *exclude[1] = { 0 } ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;

    if (!sbl_dir_get_recursive(&list, dir, exclude, FLAGS_LNK, 0))
        return -1 ;

    FOREACH_SBL(&list, pos) {

        char holder[SS_MAX_SERVICE_NAME + 1] ;
        char const *name = list.s + pos ;
        int r = service_resolve_provide(holder, name, 0, base) ;

        if (r < 0)
            return -1 ;

        if (r && (!sbl_add(names, name) || !sbl_add(holders, holder)))
            return -1 ;
    }

    return 1 ;
}

static int provide_names(strbuf *list, struct resolve_hash_s const *h)
{
    resolve_service_addon_dependencies_t const *dep = &h->dependencies ;

    if (h->res.islog || !dep->nprovide)
        return 1 ;

    return sbl_clean_string(list, dep->sa.s + dep->provide) ? 1 : -1 ;
}

int symlink_provide_parse(char const *base, hash_t *hres)
{
    log_flow() ;

    size_t pos = 0, rank = 0 ;
    struct resolve_hash_s *c, *tmp ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf holders = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf held = STRBUF_ZERO ;

    if (provide_read(&names, &holders, base) < 0)
        return -1 ;

    FOREACH_SBL(&names, pos) {

        char const *name = names.s + pos ;
        char const *holder = holders.s + rank ;
        struct resolve_hash_s *h = resolve_hash_search(hres, holder) ;
        _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

        rank += strlen(holder) + 1 ;

        if (h && provide_names(&list, h) < 0)
            return -1 ;

        if ((h && sbl_search(&list, name) < 0) || resolve_hash_search(hres, name)) {

            if (symlink_provide_drop(base, name) < 0)
                return -1 ;

            continue ;
        }

        if (!sbl_add(&held, name))
            return -1 ;
    }

    HASH_FOREACH(hres, c, tmp) {

        _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

        if (provide_names(&list, c) < 0)
            return -1 ;

        pos = 0 ;
        FOREACH_SBL(&list, pos)
            if (sbl_search(&held, list.s + pos) < 0 && provide_move(base, list.s + pos, c->name) < 0)
                return -1 ;
    }

    return 1 ;
}

int symlink_provide_enable(char const *base, struct resolve_hash_s const *h)
{
    log_flow() ;

    size_t pos = 0 ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    if (provide_names(&list, h) < 0)
        return -1 ;

    FOREACH_SBL(&list, pos) {

        char const *name = list.s + pos ;
        int r = service_resolve_provide(holder, name, 0, base) ;

        if (r < 0)
            return -1 ;

        if ((!r || strcmp(holder, h->name)) && provide_move(base, name, h->name) < 0)
            return -1 ;
    }

    return 1 ;
}

int symlink_provide_release(char const *base, char const *service)
{
    log_flow() ;

    size_t pos = 0, rank = 0 ;
    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf holders = STRBUF_ZERO ;

    if (provide_read(&names, &holders, base) < 0)
        return -1 ;

    FOREACH_SBL(&names, pos) {

        char const *holder = holders.s + rank ;

        rank += strlen(holder) + 1 ;

        if (!strcmp(holder, service) && symlink_provide_drop(base, names.s + pos) < 0)
            return -1 ;
    }

    return 1 ;
}

int symlink_provide_claim(service_graph_t const *g, char const *base, hash_t *along, char const *service, char const *name, strbuf *msg)
{
    log_flow() ;

    int r ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;

    if (service_graph_find_parsed(g, name) != SERVICE_GRAPH_NOINDEX) {

        r = service_isenabled(base, name) ;
        if (r < 0)
            return -1 ;

        if (r)
            return auto_strbuf(msg, "service: ", service, " provides the name: ", name, ", which is the name of the enabled service: ", name, " -- '66 disable -P ", name, "' first") ? 0 : -1 ;
    }

    r = service_resolve_provide(holder, name, 0, base) ;
    if (r < 0)
        return -1 ;

    if (!r || !strcmp(holder, service) || (along && resolve_hash_search(along, holder)))
        return 1 ;

    return symlink_provide_isfree(base, service, name, holder, msg) ;
}

int symlink_provide_isfree(char const *base, char const *service, char const *name, char const *holder, strbuf *msg)
{
    log_flow() ;

    int r = service_isenabled(base, holder) ;
    if (r < 0)
        return -1 ;

    if (!r)
        return 1 ;

    if (!strcmp(service, name))
        return auto_strbuf(msg, "service: ", name, " cannot be parsed, its name is provided by: ", holder, " -- '66 disable -P ", holder, "' first") ? 0 : -1 ;

    return auto_strbuf(msg, "service: ", service, " provides the name: ", name, ", already provided by the enabled: ", holder, " -- '66 disable -P ", holder, "' first") ? 0 : -1 ;
}

int symlink_provide_isavailable(char const *base, char const *name, uid_t owner, strbuf *msg)
{
    log_flow() ;

    int r ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    r = service_resolve_provide(holder, name, 0, base) ;
    if (r < 0)
        return -1 ;

    if (!r)
        return 1 ;

    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    r = service_frontend_path(&sa, name, owner, 0, exclude, 2) ;
    if (r < 0)
        return -1 ;

    if (!r)
        return auto_strbuf(msg, "name: ", name, " is provided by: ", holder, " and no service has this name -- '66 enable ", holder, "' instead") ? 0 : -1 ;

    // a frontend carries the name: a disabled provider gives it back
    r = service_isenabled(base, holder) ;
    if (r < 0)
        return -1 ;

    if (r)
        return auto_strbuf(msg, "name: ", name, " is provided by: ", holder, " -- '66 disable -P ", holder, "' first") ? 0 : -1 ;

    return 1 ;
}
