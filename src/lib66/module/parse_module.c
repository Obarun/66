/*
 * parse_module.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h> //chdir, unlink

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/directory.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h> //hiercopy

#include <66/module.h>
#include <66/resolve.h>
#include <66/info.h>
#include <66/constants.h>
#include <66/instance.h>
#include <66/utils.h>
#include <66/parse.h>
#include <66/sanitize.h>
#include <66/state.h>

static void parse_module_dependencies(stralloc *list, resolve_service_t *res, uint8_t requiredby, struct resolve_hash_s **hres, uint8_t force, uint8_t conf, ssexec_t *info)
{
    log_flow() ;

    if (!list->len)
        return ;

    char *name = res->sa.s + res->name ;
    size_t pos = 0 ;
    uint8_t opt_tree = info->opt_tree ;
    _alloc_stk_(stk, list->len + 1) ;
    _alloc_sa_(sa) ;
    uint32_t *field = !requiredby ? &res->dependencies.depends : &res->dependencies.requiredby ;
    uint32_t *nfield = !requiredby ? &res->dependencies.ndepends : &res->dependencies.nrequiredby ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    uint8_t exlen = 3 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR + 1 } ;

    info->opt_tree = 0 ;

    FOREACH_SASTR(list, pos) {

        sa.len = 0 ;
        char fname[strlen(list->s + pos)] ;

        if (!ob_basename(fname, list->s + pos))
            log_dieusys(LOG_EXIT_SYS, "basename of: ", list->s + pos) ;

        /** cannot call itself */
        if (!strcmp(name, fname))
            log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

        if (!service_frontend_path(&sa, fname, info->owner, 0, exclude, exlen))
            log_dieu(LOG_EXIT_USER, "find service frontend file of: ", fname) ;

        if (!stack_add_g(&stk, fname))
            log_dieusys(LOG_EXIT_SYS, "handle service dependencies list") ;

        (*nfield)++ ;

        parse_frontend(sa.s, hres, info, force, conf, 0, fname, 0, 0, res) ;

    }

    info->opt_tree = opt_tree ;

    if (!stack_close(&stk))
        log_dieusys(LOG_EXIT_SYS, "close stack") ;

    if (!stack_string_rebuild_with_delim(&stk, ' '))
        log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

    if (*nfield) {

        size_t len = strlen(res->sa.s + *field) ;
        char tmp[len + stk.len + 2] ;
        auto_strings(tmp, res->sa.s + *field, " ", stk.s) ;
        (*field) = resolve_add_string(wres, tmp) ;

    } else {

        (*field) = resolve_add_string(wres, stk.s) ;
    }

    free(wres) ;
}

static void parse_module_regex(resolve_service_t *res, char *dir, size_t copylen, ssexec_t *info)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    _alloc_sa_(list) ;

    /** contents */
    {
        auto_strings(dir + copylen, SS_MODULE_FRONTEND) ;

        char const *exclude[1] = { 0 } ;

        get_list(&list, dir, name, S_IFREG, exclude) ;
        regex_replace(&list, res) ;
    }

    {
        dir[copylen] = 0 ;

        char const *exclude[4] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, SS_MODULE_CONFIG_DIR + 1, 0 } ;

        /** directories */
        get_list(&list, dir, name, S_IFDIR, exclude) ;
        regex_rename(&list, res, res->regex.directories) ;

        /** filename */
        get_list(&list, dir, name, S_IFREG, exclude) ;
        regex_rename(&list, res, res->regex.files) ;
    }

    /** configure script */
    regex_configure(res, info, dir, name) ;
}

void parse_module(resolve_service_t *res, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force)
{
    log_flow() ;

    size_t pos = 0, tmplen = 0, namelen = strlen(res->sa.s + res->name) ;
    uint8_t opt_tree = info->opt_tree, conf = res->environ.env_overwrite ;
    char name[namelen + 1] ;
    char dirname[strlen(res->sa.s + res->path.frontend) + 1] ;
    _alloc_sa_(sa) ;
    _alloc_stk_(tmpdir, namelen + 12 + strlen(SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) + 1 + SS_MAX_SERVICE_NAME + 1) ;
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

    auto_strings(tmpdir.s, "/tmp/", name, ":XXXXXX") ;

    if (!mkdtemp(tmpdir.s))
        log_dieusys(LOG_EXIT_SYS, "create temporary directory") ;

    tmplen = tmpdir.len = strlen(tmpdir.s) ;

    log_trace("copy: ", dirname, " to: ", tmpdir.s) ;
    if (!hiercopy(dirname, tmpdir.s))
        log_dieusys(LOG_EXIT_SYS, "copy: ", dirname, " to: ", tmpdir.s) ;

    parse_module_regex(res, tmpdir.s, tmplen, info) ;

    /** handle new activated depends/requiredby service.*/
    {
        char const *exclude[1] = { 0 } ;
        auto_strings(tmpdir.s + tmplen, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
        get_list(&sa, tmpdir.s, name, S_IFREG, exclude) ;

        parse_module_dependencies(&sa, res, 0, hres, force, conf, info) ;

        auto_strings(tmpdir.s + tmplen, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
        get_list(&sa, tmpdir.s, name, S_IFREG, exclude) ;

        parse_module_dependencies(&sa, res, 1, hres, force, conf, info) ;
    }

    auto_strings(tmpdir.s + tmplen, SS_MODULE_ACTIVATED) ;

    {
        char const *exclude[3] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, 0 } ;

        get_list(&sa, tmpdir.s, name, S_IFREG, exclude) ;
    }

    auto_strings(tmpdir.s + tmplen, SS_MODULE_FRONTEND) ;

    {
        /* parse each activated services */
        size_t len = sa.len ;
        uint8_t exlen = 0 ; // see service_frontend_path file and compute_exclude()
        char const *exclude[1] = { 0 } ;
        _alloc_stk_(stk, len + 1) ;
        char ebase[tmplen + 1] ;
        memcpy(ebase, tmpdir.s, tmplen) ;
        ebase[tmplen] = 0 ;

        if (!ob_basename(dirname, dirname))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        if (!ob_basename(ebase, ebase))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        if (!stack_copy(&stk, sa.s, sa.len))
            log_die_nomem("stack") ;

        FOREACH_STK(&stk, pos) {

            sa.len = 0 ;
            char fname[strlen(stk.s + pos) + 1] ;

            if (!ob_basename(fname, stk.s + pos))
                log_dieusys(LOG_EXIT_ZERO, "basename of: ", stk.s + pos) ;

            /** cannot call itself */
            if (!strcmp(name, fname))
                log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

            /** Search first inside the module directory.
             * If not found, warn user about what to do.*/
            if (!service_frontend_path(&sa, fname, info->owner, tmpdir.s, exclude, exlen)) {

                tmpdir.s[tmplen] = 0 ;
                tmpdir.len = tmplen ;
                char deps[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_DEPENDS_LEN + 1 + strlen(fname) + 1] ;
                char require[tmplen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_REQUIREDBY_LEN + 1 + strlen(fname) + 1] ;

                auto_strings(deps, tmpdir.s, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS, "/", fname) ;
                auto_strings(require, tmpdir.s, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY, "/", fname) ;
                log_die(LOG_EXIT_USER, "you can not activate the service ", fname, " without providing its frontend file at ",tmpdir.s, \
                                    ". If you want to add an depends/requiredby service to the module, consider creating a named empty file at ", \
                                    deps, " or ", require) ;

            }

            info->opt_tree = 1 ;
            info->treename.len = 0 ;
            if (!auto_stra(&info->treename, res->sa.s + res->treename))
                log_die_nomem("stralloc") ;

            parse_frontend(sa.s, hres, info, force, conf, tmpdir.s, fname, name, res->intree ? res->sa.s + res->intree : 0, res) ;

            info->opt_tree = opt_tree ;
        }

    }

    /** append the module name at each inner depends/requiredby dependencies service name
     * and define contents field.*/
    parse_rename_interdependences(res, name, hres, info) ;

    /** Remove the module name from requiredby field
     * of the dependencies if the service disappears with the
     * fresh parse process.
     *
     * The Module enable the service by the configure script
     * through the activated/requiredby directory.
     * It will mark the module name as requiredby dependencies
     * at the service.
     *
     * Then the module deactivate the service. In this case
     * if the corresponding resolve field is not corrected, the sanitize_graph
     * function will found a module as requiredby field of the service
     * which is not valid at the new state of the module.
     *
     * As long as the user asked for the force option, we can retrieve
     * and read the old resolve file (meaning the current one) to
     * compare it with the new one.*/
    parse_db_migrate(res, info) ;

    /** do not die here, just warn the user */
    tmpdir.s[tmplen] = 0 ;
    log_trace("remove temporary directory: ", tmpdir.s) ;
    if (!dir_rm_rf(tmpdir.s))
        log_warnu("remove temporary directory: ", tmpdir.s) ;

    free(wres) ;
}
