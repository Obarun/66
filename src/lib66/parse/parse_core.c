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
#include <stdlib.h> // free
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

static int store_list(strbuf *stk, parse_store_t *st, uint32_t kid)
{
    size_t len = 0 ;
    char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

    if (!strbuf_copyb(stk, v, len))
        log_die_nomem("stack") ;

    return parse_list(stk) ;
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
                log_warnu_return(LOG_EXIT_ZERO, "strbuf") ;

            continue ;
        }

        if (!scan_uidlist(store->s + pos, user))
            parse_error_return(0, 0, table) ;

        if (!auto_strbuf(&sa, store->s + pos, " "))
            log_warnu_return(LOG_EXIT_ZERO, "strbuf") ;
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

/* @Return 1 on success ; 0 on error ; 2 when the frontend is a directory that is
 * not a module -- the caller lists it elsewhere and must stop parsing it. */
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
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    uint8_t opt_tree_forced = 0 ;

    if (parse_store_present(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE)) {

        table.u.parser.id = E_PARSER_SECTION_MAIN_TYPE ;

        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_TYPE, 0) ;

        int r = key_to_enum(enum_list_parser_type, v) ;
        if (r == -1) {
            free(wres) ;
            parse_error_return(0, 0, table) ;
        }

        res->type = (uint32_t)r ;

    } else log_dieu(LOG_EXIT_SYS, "get mandatory key Type of service: ", svname) ;

    /* a directory frontend that is not a module is listed by service_frontend_path,
     * not parsed here -- stop before touching the tree or the config keys. */
    if (scan_mode(sv, S_IFDIR) == 1 && res->type != E_PARSER_TYPE_MODULE) {
        free(wres) ;
        return 2 ;
    }

    if (inns) {

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns, ":", svname) ;
        res->name = resolve_add_string(wres, n) ;
        res->inns = resolve_add_string(wres, inns) ;

    } else {

        res->name = resolve_add_string(wres, svname) ;
    }

    res->owner = info->owner ;
    res->ownerstr = resolve_add_string(wres, info->ownerstr) ;
    res->path.home = resolve_add_string(wres, info->base.s) ;

    {
        char const *realname = insta > 0 ? instaname : svname ;

        if (inns) {

            char const *tmpath = strstr(sv, SS_MODULE_FRONTEND) ;
            char const *result = tmpath + SS_MODULE_FRONTEND_LEN ;
            char *path = moduleres->sa.s + moduleres->path.frontend ;
            char pdir[strlen(path)] ;
            char tdir[strlen(result)] ;

            if (!ob_dirname(pdir, path))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", path) ;

            if (!ob_dirname(tdir, result))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", result) ;

            char frontend[strlen(pdir) + SS_MODULE_FRONTEND_LEN + strlen(tdir) + strlen(realname) + 2] ;

            auto_strings(frontend, pdir, SS_MODULE_FRONTEND + 1, tdir, realname) ;

            res->path.frontend = resolve_add_string(wres, frontend) ;

        } else {

            char frontend[svlen + 1] ;

            auto_strings(frontend, svsrc, realname) ;

            res->path.frontend = resolve_add_string(wres, frontend) ;
        }
    }

    if (parse_store_present(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_INTREE)) {

        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, E_PARSER_SECTION_MAIN_INTREE, 0) ;

        res->intree = resolve_add_string(wres, v) ;

        /* an explicit InTree forces the target tree, unless a tree was already
         * forced (CLI option or the parent module) */
        if (!info->opt_tree) {
            info->treename.len = 0 ;
            info->opt_tree = 1 ;
            opt_tree_forced = 1 ;
            if (!auto_strbuf(&info->treename, res->sa.s + res->intree))
                log_die_nomem("strbuf") ;
        }
    }

    set_treeinfo(info) ;

    res->treename = resolve_add_string(wres, info->treename.s) ;

    if (inns && intree)
        res->intree = resolve_add_string(wres, intree) ;

    if (opt_tree_forced)
        info->opt_tree = 0 ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_MAIN_ENDOFKEY ; kid++) {

        if (!parse_store_present(st, E_PARSER_SECTION_MAIN, kid))
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

        switch (kid) {

            case E_PARSER_SECTION_MAIN_DESCRIPTION:

                res->description = resolve_add_string(wres, v) ;
                break ;

            case E_PARSER_SECTION_MAIN_VERSION:
                {
                    _alloc_sbl_(stk, len + 1) ;

                    if (!store_list(&stk, st, kid)) {
                        free(wres) ;
                        parse_error_return(0, 8, table) ;
                    }

                    if (stk.len > SS_SERVICE_VERSION_MAXLEN) {
                        free(wres) ;
                        parse_error_return(0, 0, table) ;
                    }

                    res->version = resolve_add_string(wres, stk.s) ;
                }
                break ;

            case E_PARSER_SECTION_MAIN_COPYFROM:
                {
                    _alloc_sbl_(stk, len + 1) ;
                    if (!store_list(&stk, st, kid)) {
                        free(wres) ;
                        parse_error_return(0, 8, table) ;
                    }

                    if (stk.len) {

                        _cleanup_strbuf_ strbuf one = STRBUF_ZERO ;

                        size_t pos = 0 ;
                        FOREACH_SBL(&stk, pos) {
                            if (!auto_strbuf(&one, stk.s + pos, " ")) {
                                free(wres) ;
                                log_die_nomem("strbuf") ;
                            }
                        }

                        one.s[one.len - 1] = 0 ;
                        res->copyfrom = resolve_add_string(wres, one.s) ;
                    }
                }
                break ;

            case E_PARSER_SECTION_MAIN_FLAGS:
                {
                    _alloc_sbl_(stk, len + 1) ;

                    if (!store_list(&stk, st, kid)) {
                        free(wres) ;
                        parse_error_return(0, 8, table) ;
                    }

                    size_t pos = 0 ;
                    FOREACH_SBL(&stk, pos) {

                        int r = key_to_enum(enum_list_parser_flags, stk.s + pos) ;

                        if (r == -1) {
                            free(wres) ;
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
                    if (!store_list(&stk, st, kid)) {
                        free(wres) ;
                        parse_error_return(0, 8, table) ;
                    }

                    if (!core_user(res, wres, &stk, table)) {
                        free(wres) ;
                        return 0 ;
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
        res->description = resolve_add_string(wres, d) ;
        log_trace("key Description at section [Main] was not set -- define it to: ", d) ;
    }

    if (!res->version) {
        res->version = resolve_add_string(wres, SS_VERSION) ;
        log_info("key Version at section [Main] was not set -- define it to: ", SS_VERSION) ;
    }

    if (!res->user) {
        if (!info->owner) {

            res->user = resolve_add_string(wres, "root") ;
            log_trace("key User at section [Main] was not set -- define it to: root") ;

        } else {

            struct passwd *pw = getpwuid(info->owner);
            if (!pw) {
                if (!errno) errno = ESRCH ;
                free(wres) ;
                log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
            }

            res->user = resolve_add_string(wres, pw->pw_name) ;
            log_trace("key User at section [Main] was not set -- define it to: ", pw->pw_name) ;
        }
    }

    res->path.servicedir = compute_src_servicedir(wres, info) ;

    res->live.livedir = resolve_add_string(wres, info->live.s) ;
    res->live.status = compute_status(wres, info) ;
    res->live.servicedir = compute_live_servicedir(wres, info) ;
    res->live.scandir = compute_scan_dir(wres, info) ;
    res->live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;
    res->live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;
    res->live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;
    res->live.fdholderdir = compute_pipe_service(wres, info, SS_FDHOLDER) ;
    res->live.oneshotddir = compute_pipe_service(wres, info, SS_ONESHOTD) ;

    free(wres) ;

    return 1 ;
}
