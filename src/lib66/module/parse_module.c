/*
 * parse_module.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>

#include <66/module.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/parse.h>
#include <66/utils.h>
#include <66/service.h>
#include <66/ssexec.h>

static int parse_module_dependencies(strbuf *list, resolve_service_t *res, resolve_service_addon_dependencies_t *dep, uint8_t requiredby, parse_build_ctx_t *ctx)
{
    log_flow() ;

    if (!list->len)
        return 1 ;

    int r ;
    char *name = res->sa.s + res->name ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
    uint32_t *field = !requiredby ? &dep->depends : &dep->requiredby ;
    uint32_t *nfield = !requiredby ? &dep->ndepends : &dep->nrequiredby ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, dep) ;

    if (*field && dep->sa.s[*field] && !sbl_clean_string(&stk, dep->sa.s + *field))
        return -1 ;

    FOREACH_SBL(list, pos) {

        char fname[strlen(list->s + pos) + 1] ;

        if (!ob_basename(fname, list->s + pos))
            return -1 ;

        /** cannot call itself */
        if (!strcmp(name, fname))
            return PARSE_REFUSE(ctx, "unable to parse module: ", name, " -- it names itself") ;

        if (!sbl_add(&stk, fname))
            return -1 ;

        (*nfield)++ ;
    }

    if (resolve_add_list(&wres, &stk, field) < 0)
        return -1 ;

    if (requiredby)
        return 1 ;

    {
        uint8_t opt_tree = ctx->info->opt_tree ;
        parse_build_ctx_t dctx = *ctx ;

        dctx.forced_directory = 0 ;
        dctx.inns = 0 ;
        dctx.intree = 0 ;
        dctx.moduleres = res ;

        ctx->info->opt_tree = 0 ;

        pos = 0 ;
        FOREACH_SBL(list, pos) {

            char fname[strlen(list->s + pos) + 1] ;

            if (!ob_basename(fname, list->s + pos))
                r = -1 ;
            else
                r = parse_dependency_name(fname, name, &dctx) ;

            if (r <= 0) {
                ctx->info->opt_tree = opt_tree ;
                return r ;
            }
        }

        ctx->info->opt_tree = opt_tree ;
    }

    return 1 ;
}

static int parse_module_regex(resolve_service_t *res, resolve_service_addon_regex_t *rx, resolve_service_addon_environ_t *e, char *dir, size_t copylen, ssexec_t *info)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->name ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    /** contents */
    {
        auto_strings(dir + copylen, SS_MODULE_FRONTEND) ;

        char const *exclude[1] = { 0 } ;

        list.len = 0 ;
        if (!sbl_dir_get_recursive(&list, dir, exclude, FLAGS_REG, 1))
            return -1 ;

        r = regex_replace(&list, rx, res) ;
        if (r <= 0)
            return r ;
    }

    {
        dir[copylen] = 0 ;

        char const *exclude[4] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, SS_MODULE_CONFIG_DIR + 1, 0 } ;

        /** directories */
        list.len = 0 ;
        if (!sbl_dir_get_recursive(&list, dir, exclude, FLAGS_DIR, 1))
            return -1 ;

        r = regex_rename(&list, rx, rx->directories) ;
        if (r <= 0)
            return r ;

        /** filename */
        list.len = 0 ;
        if (!sbl_dir_get_recursive(&list, dir, exclude, FLAGS_REG, 1))
            return -1 ;

        r = regex_rename(&list, rx, rx->files) ;
        if (r <= 0)
            return r ;
    }

    /** configure script */
    return regex_configure(res, rx, e, info, dir, name) ;
}

static int parse_module_members(strbuf *activated, resolve_service_t *res, char *tmpdir, size_t tmplen, parse_build_ctx_t *ctx)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    ssexec_t *info = ctx->info ;
    char const *name = res->sa.s + res->name ;
    uint8_t exlen = 0 ;
    char const *exclude[1] = { 0 } ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    auto_strings(tmpdir + tmplen, SS_MODULE_FRONTEND) ;

    FOREACH_SBL(activated, pos) {

        sa.len = 0 ;
        char fname[strlen(activated->s + pos) + 1] ;

        if (!ob_basename(fname, activated->s + pos))
            return -1 ;

        /** cannot call itself */
        if (!strcmp(name, fname))
            return PARSE_REFUSE(ctx, "unable to parse module: ", name, " -- it activates itself") ;

        r = service_frontend_path(&sa, fname, info->owner, tmpdir, exclude, exlen) ;
        if (r < 0)
            return -1 ;

        if (!r) {

            tmpdir[tmplen] = 0 ;
            char deps[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_DEPENDS_LEN + 1 + strlen(fname) + 1] ;
            char require[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_REQUIREDBY_LEN + 1 + strlen(fname) + 1] ;

            auto_strings(deps, tmpdir, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS, "/", fname) ;
            auto_strings(require, tmpdir, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY, "/", fname) ;

            log_warn("you can not activate the service ", fname, " without providing its frontend file at ", tmpdir, \
                ". If you want to add an depends/requiredby service to the module, consider creating a named empty file at ", \
                deps, " or ", require) ;

            return PARSE_REFUSE(ctx, "unable to parse module: ", name, " -- an activated service has no frontend in the module") ;
        }

        parse_build_ctx_t dctx = *ctx ;
        dctx.forced_directory = tmpdir ;
        dctx.inns = name ;
        dctx.intree = res->intree ? res->sa.s + res->intree : 0 ;
        dctx.moduleres = res ;

        r = parse_frontend(sa.s, dctx) ;
        if (r <= 0)
            return r ;
    }

    return 1 ;
}

int parse_module(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    int r = -1 ;
    ssexec_t *info = ctx->info ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_environ_t *e = &c->environ ;
    resolve_service_addon_dependencies_t *dep = &c->dependencies ;
    resolve_service_addon_regex_t *rx = &c->regex ;
    size_t tmplen = 0, namelen = strlen(res->sa.s + res->name) ;
    uint8_t opt_tree = info->opt_tree ;
    char name[namelen + 1] ;
    char dirname[strlen(res->sa.s + res->path.frontend) + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf treename = STRBUF_ZERO ;
    char tmpdir[5 + namelen + 7 + SS_MODULE_ACTIVATED_LEN + SS_MODULE_REQUIREDBY_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    auto_strings(name, res->sa.s + res->name) ;

    log_trace("parse module: ", name) ;

    if (!ob_dirname(dirname, res->sa.s + res->path.frontend))
        return -1 ;

    {
        _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;

        if (!dir_create_tmp(&tmp, "/tmp", name))
            return -1 ;

        auto_strings(tmpdir, tmp.s) ;
        tmplen = strlen(tmpdir) ;
    }

    log_trace("copy: ", dirname, " to: ", tmpdir) ;
    if (!tree_copy(dirname, tmpdir))
        goto end ;

    /** check mandatory directories */
    if ((r = parse_module_check_dir(tmpdir, SS_MODULE_CONFIG_DIR)) <= 0 ||
        (r = parse_module_check_dir(tmpdir, SS_MODULE_ACTIVATED)) <= 0 ||
        (r = parse_module_check_dir(tmpdir, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS)) <= 0 ||
        (r = parse_module_check_dir(tmpdir, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY)) <= 0 ||
        (r = parse_module_check_dir(tmpdir, SS_MODULE_FRONTEND)) <= 0)
            goto end ;

    r = parse_module_regex(res, rx, e, tmpdir, tmplen, info) ;
    if (r <= 0)
        goto end ;

    {
        char const *exclude[1] = { 0 } ;

        auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
        sa.len = 0 ;
        if (!sbl_dir_get_recursive(&sa, tmpdir, exclude, FLAGS_REG, 1)) {
            r = -1 ;
            goto end ;
        }

        r = parse_module_dependencies(&sa, res, dep, 0, ctx) ;
        if (r <= 0)
            goto end ;

        auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
        sa.len = 0 ;
        if (!sbl_dir_get_recursive(&sa, tmpdir, exclude, FLAGS_REG, 1)) {
            r = -1 ;
            goto end ;
        }

        r = parse_module_dependencies(&sa, res, dep, 1, ctx) ;
        if (r <= 0)
            goto end ;
    }

    auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED) ;

    {
        char const *exclude[3] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, 0 } ;

        sa.len = 0 ;
        if (!sbl_dir_get_recursive(&sa, tmpdir, exclude, FLAGS_REG, 1)) {
            r = -1 ;
            goto end ;
        }
    }

    if (!auto_strbuf(&treename, info->treename.s ? info->treename.s : "")) {
        r = -1 ;
        goto end ;
    }

    info->opt_tree = 1 ;
    info->treename.len = 0 ;
    if (!auto_strbuf(&info->treename, res->sa.s + res->treename)) {
        r = -1 ;
    } else {
        r = parse_module_members(&sa, res, tmpdir, tmplen, ctx) ;
    }

    info->opt_tree = opt_tree ;
    info->treename.len = 0 ;
    if (!auto_strbuf(&info->treename, treename.s) && r > 0)
        r = -1 ;

    if (r <= 0)
        goto end ;

    r = parse_rename_interdependences(dep, name, ctx->hres, info) ;

    end:
        tmpdir[tmplen] = 0 ;
        tmpdir_destroy(tmpdir) ;
        return r ;
}
