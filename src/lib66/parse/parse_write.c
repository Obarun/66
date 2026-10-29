/*
 * parse_write.c
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
#include <sys/stat.h> // chmod

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/directory.h>
#include <oblibs/hash.h>

#include <66/constants.h>
#include <66/parse.h>
#include <66/utils.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/symlink.h>
#include <66/state.h>
#include <66/tree.h>
#include <66/write.h>
#include <66/environ.h>

static int keep_before(struct resolve_hash_s *c, strbuf *oldtree, char const *base)
{
    int r = 1 ;
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    resolve_service_t const *before = &c->before ;

    if (!c->beforeread) {

        r = service_read(&h, base, c->name, 0) ;
        if (r < 0)
            return -1 ;

        before = &h.res ;
    }

    if (!before->sa.len)
        return 1 ;

    c->res.enabled = before->enabled ;

    if (before->treename && !auto_strbuf(oldtree, before->sa.s + before->treename))
        r = -1 ;

    service_read_free(&h) ;

    return r ;
}

static int write_resolve(struct resolve_hash_s *c, char const *dir, char const *tmpdir)
{
    size_t pos = 0 ;
    char resolve[strlen(dir) + SS_RESOLVE_LEN + 1] ;

    struct { uint32_t has ; uint8_t type ; void *obj ; } addon[] = {
        { 1, DATA_SERVICE, &c->res },
        { c->res.has_limit, DATA_SERVICE_LIMIT, &c->limit },
        { c->res.has_environ, DATA_SERVICE_ENVIRON, &c->environ },
        { c->res.has_io, DATA_SERVICE_IO, &c->io },
        { c->res.has_execute, DATA_SERVICE_EXECUTE, &c->execute },
        { c->res.has_dependencies, DATA_SERVICE_DEPENDENCIES, &c->dependencies },
        { c->res.has_regex, DATA_SERVICE_REGEX, &c->regex },
        { c->res.has_event, DATA_SERVICE_EVENT, &c->event },
    } ;

    auto_strings(resolve, dir, SS_RESOLVE) ;

    if (!dir_create_parent(resolve, 0755))
        return -1 ;

    for (; pos < sizeof(addon) / sizeof(addon[0]) ; pos++) {

        resolve_wrapper_t w = RESOLVE_WRAPPER(addon[pos].type, addon[pos].obj) ;

        if (!addon[pos].has)
            continue ;

        log_trace("write resolve file: ", resolve, "/", c->name, resolve_addon_suffix(addon[pos].type)) ;
        if (!resolve_write_at(&w, dir, c->name, tmpdir))
            return -1 ;
    }

    return 1 ;
}


static int write_conf(struct resolve_hash_s *c)
{
    int r ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_environ_t *e = &c->environ ;

    if (!res->has_environ || !e->env)
        return 1 ;

    char const *svconf = e->sa.s + e->envdir ;
    char const *version = res->sa.s + res->version ;
    _cleanup_strbuf_ strbuf previous = STRBUF_ZERO ;
    char dir[strlen(svconf) + 1 + strlen(version) + 1] ;

    // the version the link leads to before the switch
    r = env_find_current_version(&previous, svconf) ;
    if (r < 0)
        return -1 ;

    char pv[previous.len + 1] ;

    if (r && !ob_basename(pv, previous.s))
        return -1 ;

    auto_strings(dir, svconf, "/", version) ;

    if (!dir_create_parent(dir, 0755) || env_write_version(dir, r ? pv : 0, res, e) < 0)
        return -1 ;

    return env_make_symlink(res, e) ;
}

static int write_servicedir(struct resolve_hash_s *c, char const *base, char const *servicedir)
{
    ss_state_t sta = STATE_ZERO ;
    size_t baselen = strlen(base) ;
    char tmpdir[TMPDIR_GETLEN(baselen) + 1] ;
    char parent[strlen(servicedir) + 1] ;
    _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;

    auto_strings(tmpdir, base, SS_TMPDIR + 1) ;

    if (!ob_dirname(parent, servicedir) || !dir_create_parent(parent, 0755))
        return -1 ;

    if (!dir_create_tmp(&tmp, tmpdir, c->name))
        return -1 ;

    char state[tmp.len + SS_STATE_LEN + 1] ;
    auto_strings(state, tmp.s, SS_STATE) ;

    state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_TRUE) ;

    if (write_services(&c->res, &c->execute, &c->copyfrom, tmp.s) < 0 ||
        !state_write_remote(&sta, state) ||
        write_resolve(c, tmp.s, tmpdir) < 0 ||
        chmod(tmp.s, 0755) < 0 ||
        !dir_replace_at(tmpdir, tmp.s, servicedir)) {
            tmpdir_destroy(tmp.s) ;
            return -1 ;
    }

    return 1 ;
}

static int write_one(struct resolve_hash_s *c, ssexec_t *info, strbuf *adds, strbuf *removes)
{
    log_flow() ;

    int r ;
    char const *base = info->base.s ;
    char servicedir[strlen(c->res.sa.s + c->res.path.servicedir) + 1] ;
    char tree[strlen(c->res.sa.s + c->res.treename) + 1] ;
    _cleanup_strbuf_ strbuf oldtree = STRBUF_ZERO ;
    size_t namelen = strlen(c->name) ;
    char link[strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char dir[sizeof(link)] ;

    auto_strings(servicedir, c->res.sa.s + c->res.path.servicedir) ;
    auto_strings(tree, c->res.sa.s + c->res.treename) ;

    if (keep_before(c, &oldtree, base) < 0 || write_conf(c) < 0 || write_servicedir(c, base, servicedir) < 0)
        return -1 ;

    char tmpdir[TMPDIR_GETLEN(strlen(base)) + 1] ;

    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(link, dir, "/", c->name) ;
    auto_strings(tmpdir, base, SS_TMPDIR + 1) ;

    if (!dir_create_parent(dir, 0755) || symlink_atomic_at(tmpdir, servicedir, link) < 0)
        return -1 ;

    r = tree_isvalid(base, tree) ;
    if (r < 0)
        return -1 ;

    if (!r) {

        if (tree_service_add(tree, c->name, info) < 0)
            return -1 ;

    } else if (!sbl_add(adds, tree) || !sbl_add(adds, c->name))
        return -1 ;

    if (oldtree.len && strcmp(oldtree.s, tree) && (!sbl_add(removes, oldtree.s) || !sbl_add(removes, c->name)))
        return -1 ;

    log_info("Parsed successfully: ", c->name, " at tree: ", tree) ;

    return 1 ;
}

int parse_write(hash_t *hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    _cleanup_strbuf_ strbuf adds = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf removes = STRBUF_ZERO ;

    HASH_FOREACH(hres, c, tmp) {
        if (write_one(c, info, &adds, &removes) < 0)
            return -1 ;
    }

    if (tree_contents_batch(info->base.s, &adds, true) < 0 || tree_contents_batch(info->base.s, &removes, false) < 0)
        return -1 ;

    return symlink_provide_parse(info->base.s, hres) ;
}
