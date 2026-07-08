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
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>

#include <66/module.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/parse.h>
#include <66/service.h>
#include <66/ssexec.h>

static void parse_module_dependencies(strbuf *list, resolve_service_t *res, resolve_service_addon_dependencies_t *dep, uint8_t requiredby, parse_build_ctx_t *ctx)
{
    log_flow() ;

    if (!list->len)
        return ;

    char *name = res->sa.s + res->name ;
    size_t pos = 0 ;
    uint8_t opt_tree = ctx->info->opt_tree ;
    _alloc_sbl_(stk, list->len + 1) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint32_t *field = !requiredby ? &dep->depends : &dep->requiredby ;
    uint32_t *nfield = !requiredby ? &dep->ndepends : &dep->nrequiredby ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
    uint8_t exlen = 3 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR + 1 } ;

    ctx->info->opt_tree = 0 ;

    FOREACH_SBL(list, pos) {

        sa.len = 0 ;
        char fname[strlen(list->s + pos)] ;

        if (!ob_basename(fname, list->s + pos))
            log_dieusys(LOG_EXIT_SYS, "basename of: ", list->s + pos) ;

        /** cannot call itself */
        if (!strcmp(name, fname))
            log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

        if (!service_frontend_path(&sa, fname, ctx->info->owner, 0, exclude, exlen))
            log_dieu(LOG_EXIT_USER, "find service frontend file of: ", fname) ;

        if (!sbl_add(&stk, fname))
            log_dieusys(LOG_EXIT_SYS, "handle service dependencies list") ;

        (*nfield)++ ;

        parse_build_ctx_t dctx = *ctx ;
        dctx.forced_directory = 0 ;
        dctx.main = fname ;
        dctx.inns = 0 ;
        dctx.intree = 0 ;
        dctx.moduleres = res ;
        parse_frontend(sa.s, dctx) ;

    }

    ctx->info->opt_tree = opt_tree ;

    if (!sbl_rebuild_with_delim(&stk, ' '))
        log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

    if (*nfield) {

        size_t len = strlen(dep->sa.s + *field) ;
        char tmp[len + stk.len + 2] ;
        auto_strings(tmp, dep->sa.s + *field, " ", stk.s) ;
        (*field) = resolve_add_string(wres, tmp) ;

    } else {

        (*field) = resolve_add_string(wres, stk.s) ;
    }

    free(wres) ;
}

static void parse_module_regex(resolve_service_t *res, resolve_service_addon_regex_t *rx, resolve_service_addon_environ_t *e, char *dir, size_t copylen, ssexec_t *info)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    /** contents */
    {
        auto_strings(dir + copylen, SS_MODULE_FRONTEND) ;

        char const *exclude[1] = { 0 } ;

        get_list(&list, dir, name, S_IFREG, exclude) ;
        regex_replace(&list, rx, res) ;
    }

    {
        dir[copylen] = 0 ;

        char const *exclude[4] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, SS_MODULE_CONFIG_DIR + 1, 0 } ;

        /** directories */
        get_list(&list, dir, name, S_IFDIR, exclude) ;
        regex_rename(&list, rx, rx->directories) ;

        /** filename */
        get_list(&list, dir, name, S_IFREG, exclude) ;
        regex_rename(&list, rx, rx->files) ;
    }

    /** configure script */
    regex_configure(res, rx, e, info, dir, name) ;
}

void parse_module(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    ssexec_t *info = ctx->info ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_environ_t *e = &c->environ ;
    resolve_service_addon_dependencies_t *dep = &c->dependencies ;
    resolve_service_addon_regex_t *rx = &c->regex ;
    size_t pos = 0, tmplen = 0, namelen = strlen(res->sa.s + res->name) ;
    uint8_t opt_tree = info->opt_tree ;
    char name[namelen + 1] ;
    char dirname[strlen(res->sa.s + res->path.frontend) + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char tmpdir[namelen + 12 + strlen(SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) + 1 + SS_MAX_SERVICE_NAME + 1] ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    auto_strings(name,res->sa.s + res->name) ;

    log_trace("parse module: ", name) ;

    if (!ob_dirname(dirname, res->sa.s + res->path.frontend))
        log_dieu(LOG_EXIT_SYS, "get directory name of: ", res->sa.s + res->path.frontend) ;

    /** check mandatory directories */
    parse_module_check_dir(dirname, SS_MODULE_CONFIG_DIR) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
    parse_module_check_dir(dirname, SS_MODULE_FRONTEND) ;

    auto_strings(tmpdir, "/tmp/", name, ":XXXXXX") ;

    if (!mkdtemp(tmpdir))
        log_dieusys(LOG_EXIT_SYS, "create temporary directory") ;

    tmplen = strlen(tmpdir) ;

    log_trace("copy: ", dirname, " to: ", tmpdir) ;
    if (!tree_copy(dirname, tmpdir))
        log_dieusys(LOG_EXIT_SYS, "copy: ", dirname, " to: ", tmpdir) ;

    parse_module_regex(res, rx, e, tmpdir, tmplen, info) ;

    /** handle new activated depends/requiredby service.*/
    {
        char const *exclude[1] = { 0 } ;
        auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
        get_list(&sa, tmpdir, name, S_IFREG, exclude) ;

        parse_module_dependencies(&sa, res, dep, 0, ctx) ;

        auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
        get_list(&sa, tmpdir, name, S_IFREG, exclude) ;

        parse_module_dependencies(&sa, res, dep, 1, ctx) ;
    }

    auto_strings(tmpdir + tmplen, SS_MODULE_ACTIVATED) ;

    {
        char const *exclude[3] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, 0 } ;

        get_list(&sa, tmpdir, name, S_IFREG, exclude) ;
    }

    auto_strings(tmpdir + tmplen, SS_MODULE_FRONTEND) ;

    {
        /* parse each activated services */
        size_t len = sa.len ;
        uint8_t exlen = 0 ; // see service_frontend_path file and compute_exclude()
        char const *exclude[1] = { 0 } ;
        _alloc_sbl_(stk, len + 1) ;

        if (!strbuf_copyb(&stk, sa.s, sa.len))
            log_die_nomem("strbuf") ;

        FOREACH_SBL(&stk, pos) {

            sa.len = 0 ;
            char fname[strlen(stk.s + pos) + 1] ;

            if (!ob_basename(fname, stk.s + pos))
                log_dieusys(LOG_EXIT_ZERO, "basename of: ", stk.s + pos) ;

            /** cannot call itself */
            if (!strcmp(name, fname))
                log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

            /** Search first inside the module directory.
             * If not found, warn user about what to do.*/
            if (!service_frontend_path(&sa, fname, info->owner, tmpdir, exclude, exlen)) {

                tmpdir[tmplen] = 0 ;
                char deps[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_DEPENDS_LEN + 1 + strlen(fname) + 1] ;
                char require[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_REQUIREDBY_LEN + 1 + strlen(fname) + 1] ;

                auto_strings(deps, tmpdir, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS, "/", fname) ;
                auto_strings(require, tmpdir, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY, "/", fname) ;
                log_die(LOG_EXIT_USER, "you can not activate the service ", fname, " without providing its frontend file at ",tmpdir, \
                                    ". If you want to add an depends/requiredby service to the module, consider creating a named empty file at ", \
                                    deps, " or ", require) ;

            }

            info->opt_tree = 1 ;
            info->treename.len = 0 ;
            if (!auto_strbuf(&info->treename, res->sa.s + res->treename))
                log_die_nomem("strbuf") ;

            parse_build_ctx_t dctx = *ctx ;
            dctx.forced_directory = tmpdir ;
            dctx.main = fname ;
            dctx.inns = name ;
            dctx.intree = res->intree ? res->sa.s + res->intree : 0 ;
            dctx.moduleres = res ;
            parse_frontend(sa.s, dctx) ;

            info->opt_tree = opt_tree ;
        }

    }

    /** append the module name at each inner depends/requiredby dependencies service name
     * and define contents field.*/
    parse_rename_interdependences(res, dep, name, ctx->hres, info) ;

    /** Remove the module name from requiredby field
     * of the dependencies if the service disappears with the
     * fresh parse process.
     *
     * The Module enable the service by the configure script
     * through the activated/requiredby directory.
     * It will mark the module name as requiredby dependencies
     * for the service.
     *
     * Then the module deactivate the service. In this case
     * if the corresponding service resolve field is not corrected, the sanitize_graph
     * function will found a module name as requiredby by the service
     * which is not valid with the new state of the module.
     *
     * As long as the user asked for the force option, we can retrieve
     * and read the old resolve file (meaning the current in use) to
     * compare it with the new one.*/
    parse_db_migrate(res, dep, info) ;

    /** do not die here, just warn the user */
    tmpdir[tmplen] = 0 ;
    log_trace("remove temporary directory: ", tmpdir) ;
    if (!dir_destroy(tmpdir))
        log_warnu("remove temporary directory: ", tmpdir) ;

    free(wres) ;
}
