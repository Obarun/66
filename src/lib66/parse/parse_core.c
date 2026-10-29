/*
 * parse_core.c
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
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h> // S_IFDIR

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/account.h>
#include <oblibs/types.h>
#include <oblibs/files.h> // scan_mode

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/module.h>
#include <66/tree.h>

static int store_list(strbuf *stk, parse_store_t *st, uint32_t kid)
{
    size_t len = 0 ;
    char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

    if (!strbuf_copyb(stk, v, len))
        return -1 ;

    return parse_list(stk) ;
}

static int core_tree(strbuf *tree, char const *name, char const *intree, ssexec_t *info)
{
    int r ;
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;

    if (info->opt_tree) {

        if (!auto_strbuf(tree, info->treename.s))
            return -1 ;

    } else {

        r = service_read(&h, info->base.s, name, 0) ;
        if (r < 0)
            return -1 ;

        if (r && h.res.treename && h.res.sa.s[h.res.treename]) {

            r = auto_strbuf(tree, h.res.sa.s + h.res.treename) ;
            service_read_free(&h) ;
            if (!r)
                return -1 ;

        } else if (intree) {

            service_read_free(&h) ;

            if (!auto_strbuf(tree, intree))
                return -1 ;

        } else {

            char t[SS_MAX_TREENAME + 1] ;

            service_read_free(&h) ;

            if (tree_find_current(t, info->base.s) < 0 || !auto_strbuf(tree, t))
                return -1 ;
        }
    }

    r = tree_isvalid(info->base.s, tree->s) ;
    if (r < 0)
        return -1 ;

    if (r) {
        r = tree_get_permissions(info->base.s, tree->s) ;
        if (r < 0)
            return -1 ;
        if (!r)
            log_warn_return(LOG_EXIT_ZERO, "You're not allowed to use the tree: ", tree->s) ;
    }

    return 1 ;
}

static int core_user(resolve_service_t *res, resolve_wrapper_t_ref wres, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    if (!store->len)
        return 1 ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t pos = 0 ;

    uid_t user[256] ;
    memset(user, 0, 256 * sizeof(uid_t)) ;

    uid_t owner = MYUID ;
    if (!owner) {
        if (sbl_search(store, "root") == -1)
            log_warnu_return(LOG_EXIT_ZERO, "use the service -- permission denied") ;
    }
    /** special case, we don't know which user want to use the service, we need a
     * general name to allow the current owner of the process. The term "user" is
     * took here to allow him */
    ssize_t p = sbl_search(store, "user") ;

    FOREACH_SBL(store, pos) {

        if (pos == (size_t)p) {

            if (!owner)
                /** avoid field e.g root root where originaly we want e.g. user
                 * root. The term user will be root at getpwuid() call */
                continue ;

            struct passwd *pw = getpwuid(owner);
            if (!pw) {
                if (!errno) errno = ESRCH ;
                log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
            }

            if (!scan_uidlist(pw->pw_name, user))
                parse_error_return(0, 0, table) ;

            if (!auto_strbuf(&sa, pw->pw_name, " "))
                return -1 ;

            continue ;
        }

        if (!scan_uidlist(store->s + pos, user))
            parse_error_return(0, 0, table) ;

        if (!auto_strbuf(&sa, store->s + pos, " "))
            return -1 ;
    }

    int nb = (int)user[0] ;
    if (p == -1 && owner) {

        int e = 0 ;
        for (int i = 1; i < nb+1; i++) {
            if (user[i] == owner) {
                e = 1 ;
                break ;
            }
        }
        if (!e)
            log_warnu_return(LOG_EXIT_ZERO,"use the service -- permission denied") ;
    }

    res->user = resolve_add_string(wres, sa.s) ;

    return 1 ;
}

int parse_core(resolve_service_t *res, char const *sv, char const *svname,
               char const *svsrc, size_t svlen, int insta, char const *instaname,
               parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    ssexec_t *info = ctx->info ;
    char const *inns = ctx->inns ;
    char const *intree = ctx->intree ;
    resolve_service_t *moduleres = ctx->moduleres ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, res) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    _cleanup_strbuf_ strbuf tree = STRBUF_ZERO ;

    if (parse_store_present(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE)) {

        table.u.parser.id = E_PARSER_SECTION_MAIN_TYPE ;

        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE, 0) ;

        int r = key_to_enum(enum_list_parser_type, v) ;
        if (r == -1) {
            parse_error_return(0, 0, table) ;
        }

        res->type = (uint32_t)r ;

    } else {

        log_warnu_return(LOG_EXIT_ZERO, "get mandatory key Type of service: ", svname) ;
    }

    /* a directory frontend that is not a module is listed by service_frontend_path,
     * not parsed here -- stop before touching the tree or the config keys. */
    ssize_t isdir = scan_mode(sv, FLAGS_DIR) ;
    if (!isdir && errno != ENOENT) {
        return -1 ;
    }

    if (isdir == 1 && res->type != E_PARSER_TYPE_MODULE) {
        return 2 ;
    }

    if (inns) {

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns, ":", svname) ;
        res->name = resolve_add_string(&wres, n) ;
        res->inns = resolve_add_string(&wres, inns) ;

    } else {

        res->name = resolve_add_string(&wres, svname) ;
    }

    res->owner = info->owner ;
    res->ownerstr = resolve_add_string(&wres, info->ownerstr) ;
    res->path.home = resolve_add_string(&wres, info->base.s) ;

    {
        char const *realname = insta > 0 ? instaname : svname ;

        if (inns) {

            char const *tmpath = strstr(sv, SS_MODULE_FRONTEND) ;
            char const *result = tmpath + SS_MODULE_FRONTEND_LEN ;
            char *path = moduleres->sa.s + moduleres->path.frontend ;
            char pdir[strlen(path)] ;
            char tdir[strlen(result)] ;

            if (!ob_dirname(pdir, path) || !ob_dirname(tdir, result)) {
                return -1 ;
            }

            char frontend[strlen(pdir) + SS_MODULE_FRONTEND_LEN + strlen(tdir) + strlen(realname) + 2] ;

            auto_strings(frontend, pdir, SS_MODULE_FRONTEND + 1, tdir, realname) ;

            res->path.frontend = resolve_add_string(&wres, frontend) ;

        } else {

            char frontend[svlen + 1] ;

            auto_strings(frontend, svsrc, realname) ;

            res->path.frontend = resolve_add_string(&wres, frontend) ;
        }
    }

    if (parse_store_present(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_INTREE)) {

        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_INTREE, 0) ;

        res->intree = resolve_add_string(&wres, v) ;
    }

    /** an explicit InTree forces the target tree, unless a tree was already
     * forced (CLI option or the parent module) */
    {
        int r = core_tree(&tree, res->sa.s + res->name, res->intree ? res->sa.s + res->intree : 0, info) ;
        if (r <= 0) {
            return r ;
        }
    }

    res->treename = resolve_add_string(&wres, tree.s) ;

    if (inns && intree)
        res->intree = resolve_add_string(&wres, intree) ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_MAIN_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_MAIN, kid))
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

        switch (kid) {

            case E_PARSER_SECTION_MAIN_DESCRIPTION:

                res->description = resolve_add_string(&wres, v) ;
                break ;

            case E_PARSER_SECTION_MAIN_VERSION:
                {
                    _alloc_sbl_(stk, len + 1) ;

                    int r = store_list(&stk, st, kid) ;
                    if (r <= 0) {
                        if (r < 0)
                            return -1 ;
                        parse_error_return(0, 8, table) ;
                    }

                    if (stk.len > SS_SERVICE_VERSION_MAXLEN) {
                        parse_error_return(0, 0, table) ;
                    }

                    res->version = resolve_add_string(&wres, stk.s) ;
                }
                break ;

            case E_PARSER_SECTION_MAIN_COPYFROM:
                {
                    _alloc_sbl_(stk, len + 1) ;
                    int r = store_list(&stk, st, kid) ;
                    if (r <= 0) {
                        if (r < 0)
                            return -1 ;
                        parse_error_return(0, 8, table) ;
                    }

                    if (stk.len) {

                        _cleanup_strbuf_ strbuf one = STRBUF_ZERO ;

                        size_t pos = 0 ;
                        FOREACH_SBL(&stk, pos) {

                            char src[SS_MAX_PATH_LEN + 1] ;
                            char dname[SS_MAX_PATH_LEN + 1] ;

                            r = parse_copyfrom_path(src, dname, res->sa.s + res->path.frontend, stk.s + pos) ;
                            if (r <= 0) {
                                if (r < 0)
                                    return -1 ;
                                log_warn("path of key: CopyFrom: does not exist: ", stk.s + pos) ;
                                parse_error_return(0, 0, table) ;
                            }

                            if (!sbl_add(ctx->copyfrom, src) || !sbl_add(ctx->copyfrom, dname) ||
                                !auto_strbuf(&one, stk.s + pos, " ")) {
                                return -1 ;
                            }
                        }

                        one.s[one.len - 1] = 0 ;
                        res->copyfrom = resolve_add_string(&wres, one.s) ;
                    }
                }
                break ;

            case E_PARSER_SECTION_MAIN_FLAGS:
                {
                    _alloc_sbl_(stk, len + 1) ;

                    int r = store_list(&stk, st, kid) ;
                    if (r <= 0) {
                        if (r < 0)
                            return -1 ;
                        parse_error_return(0, 8, table) ;
                    }

                    size_t pos = 0 ;
                    FOREACH_SBL(&stk, pos) {

                        r = key_to_enum(enum_list_parser_flags, stk.s + pos) ;

                        if (r == -1) {
                            parse_error_return(0, 0, table) ;
                        }

                        if (r == E_PARSER_FLAGS_EARLIER)
                            res->earlier = 1 ; // DOWN is parse_execute's
                    }
                }
                break ;

            case E_PARSER_SECTION_MAIN_USER:
                {
                    _alloc_sbl_(stk, len + 1) ;
                    int r = store_list(&stk, st, kid) ;
                    if (r <= 0) {
                        if (r < 0)
                            return -1 ;
                        parse_error_return(0, 8, table) ;
                    }

                    r = core_user(res, &wres, &stk, table) ;
                    if (r <= 0) {
                        return r ;
                    }
                }
                break ;

            default:
                // Type/InTree handled above ; execute/dependencies/io keys aren't core's
                break ;
        }
    }

    // mandatory-field defaults
    if (!res->description) {
        char d[strlen(res->sa.s + res->name) + 8 + 1] ;
        auto_strings(d, res->sa.s + res->name, " service") ;
        res->description = resolve_add_string(&wres, d) ;
        log_trace("key Description at section [Main] was not set -- define it to: ", d) ;
    }

    if (!res->version) {
        res->version = resolve_add_string(&wres, SS_VERSION) ;
        log_trace("key Version at section [Main] was not set -- define it to: ", SS_VERSION) ;
    }

    if (!res->user) {
        if (!info->owner) {

            res->user = resolve_add_string(&wres, "root") ;
            log_trace("key User at section [Main] was not set -- define it to: root") ;

        } else {

            struct passwd *pw = getpwuid(info->owner);
            if (!pw) {
                if (!errno) errno = ESRCH ;
                log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
            }

            res->user = resolve_add_string(&wres, pw->pw_name) ;
            log_trace("key User at section [Main] was not set -- define it to: ", pw->pw_name) ;
        }
    }

    res->path.servicedir = compute_src_servicedir(&wres, info) ;

    if (parse_compute_live(&wres, info) < 0) {
        return -1 ;
    }

    return 1 ;
}
