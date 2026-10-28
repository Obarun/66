/*
 * service_remove_one.c
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
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/symlink.h>
#include <66/tree.h>
#include <66/utils.h>
#include <66/environ.h>
#include <66/parse.h>
#include <66/enum_parser.h>

static int destroy(char const *path)
{
    log_trace("remove: ", path) ;
    return dir_destroy(path) ? 1 : -1 ;
}

static int read_file(void *obj, uint8_t type, char const *base, char const *name)
{
    resolve_wrapper_t w = RESOLVE_WRAPPER(type, obj) ;

    return resolve_read(&w, base, name) ;
}

static int core_of(resolve_service_t *res, char const *base, char const *name, uid_t owner)
{
    int r = read_file(res, DATA_SERVICE, base, name) ;
    if (r)
        return r ;

    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, res) ;
    ssize_t off ;

    resolve_free_obj(&w) ;
    *res = (resolve_service_t)RESOLVE_SERVICE_ZERO ;
    resolve_init(&w) ;

    off = resolve_add_string(&w, name) ;
    if (off < 0)
        return -1 ;

    res->name = (uint32_t)off ;
    res->owner = owner ;

    return 0 ;
}

static int destroy_logdir(resolve_service_t *res, int readable, char const *base, char const *name)
{
    int r = 0, e = -1 ;
    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_IO, &io) ;

    if (readable) {

        if (!res->has_io)
            return 1 ;

        r = read_file(&io, DATA_SERVICE_IO, base, name) ;
        if (r < 0)
            goto freed ;
    }

    if (r) {

        if (io.fdout.type == E_PARSER_IO_TYPE_66LOG && io.fdout.destination && destroy(io.sa.s + io.fdout.destination) < 0)
            goto freed ;

    } else if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0) {

        resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, res) ;
        ssize_t off = compute_log_dir(&wres, res, 0) ;

        if (off < 0 || destroy(res->sa.s + off) < 0)
            goto freed ;
    }

    e = 1 ;

    freed:
        resolve_free_obj(&w) ;
        return e ;
}

static int destroy_envdir(resolve_service_t *res, int readable, char const *base, char const *name)
{
    int r = 0, e = -1 ;
    resolve_service_addon_environ_t env = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_ENVIRON, &env) ;
    _cleanup_strbuf_ strbuf conf = STRBUF_ZERO ;

    if (readable) {

        if (!res->has_environ)
            return 1 ;

        r = read_file(&env, DATA_SERVICE_ENVIRON, base, name) ;
        if (r < 0)
            goto freed ;
    }

    if (r) {

        if (env.envdir && destroy(env.sa.s + env.envdir) < 0)
            goto freed ;

    } else if (!env_resolve_conf(&conf, res) || destroy(conf.s) < 0)
        goto freed ;

    e = 1 ;

    freed:
        resolve_free_obj(&w) ;
        return e ;
}

static int destroy_one(char const *base, uid_t owner, char const *name)
{
    log_flow() ;

    int r, e = -1 ;
    size_t baselen = strlen(base), namelen = strlen(name) ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
    _cleanup_strbuf_ strbuf tree = STRBUF_ZERO ;
    char link[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;
    char svc[baselen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

    int readable = core_of(&res, base, name, owner) ;
    if (readable < 0)
        goto freed ;

    if (destroy_logdir(&res, readable, base, name) < 0 || destroy_envdir(&res, readable, base, name) < 0)
        goto freed ;

    if (symlink_provide_release(base, name) < 0)
        goto freed ;

    if (readable && res.treename && res.sa.s[res.treename]) {

        if (!auto_strbuf(&tree, res.sa.s + res.treename))
            goto freed ;

    } else {

        r = tree_find_service(&tree, base, name) ;
        if (r < 0)
            goto freed ;
    }

    if (tree.len && tree_service_remove(base, tree.s, name) < 0)
        goto freed ;

    auto_strings(link, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    if (readable) {

        auto_strings(svc, res.sa.s + res.path.servicedir) ;

    } else {

        auto_strings(svc, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    }

    if (destroy(link) < 0 || destroy(svc) < 0)
        goto freed ;

    e = 1 ;

    freed:
        resolve_free_obj(&w) ;
        return e ;
}

static int destroy_with_logger(char const *base, uid_t owner, char const *name)
{
    int r, has = 0 ;
    size_t namelen = strlen(name) ;
    char logger[namelen + SS_LOG_SUFFIX_LEN + 1] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    auto_strings(logger, name, SS_LOG_SUFFIX) ;

    r = read_file(&res, DATA_SERVICE, base, name) ;
    if (r < 0)
        goto err ;

    if (r) {

        has = res.logger ? 1 : 0 ;

    } else {

        resolve_free_obj(&w) ;
        res = (resolve_service_t)RESOLVE_SERVICE_ZERO ;

        r = read_file(&res, DATA_SERVICE, base, logger) ;
        if (r < 0)
            goto err ;

        if (r) {
            has = res.islog ? 1 : 0 ;
        } else if (errno == EINVAL) {
            has = 1 ;
        }
    }

    resolve_free_obj(&w) ;

    if (has && destroy_one(base, owner, logger) < 0)
        return -1 ;

    return destroy_one(base, owner, name) ;

    err:
        resolve_free_obj(&w) ;
        return -1 ;
}

int service_remove_one(char const *base, uid_t owner, char const *name)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf members = STRBUF_ZERO ;
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &dep) ;
    resolve_enum_table_t table = E_TABLE_SERVICE_DEPS_ZERO ;

    table.u.service.id = E_RESOLVE_SERVICE_DEPS_CONTENTS ;

    r = resolve_get_field(&members, &w, base, name, table) ;

    resolve_free_obj(&w) ;

    if (r < 0)
        return -1 ;

    FOREACH_SBL(&members, pos) {

        if (destroy_with_logger(base, owner, members.s + pos) < 0)
            return -1 ;
    }

    return destroy_with_logger(base, owner, name) ;
}
