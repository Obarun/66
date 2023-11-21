/*
 * parse_module.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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

static void parse_module_dependencies(stralloc *list, resolve_service_t *res, uint8_t requiredby, resolve_service_t *ares, unsigned int *areslen, uint8_t force, uint8_t conf, ssexec_t *info)
{
    log_flow() ;

    if (!list->len)
        return ;

    char *name = res->sa.s + res->name ;
    size_t pos = 0 ;
    uint8_t opt_tree = info->opt_tree ;
    _init_stack_(stk, list->len + 1) ;
    uint32_t *field = !requiredby ? &res->dependencies.depends : &res->dependencies.requiredby ;
    uint32_t *nfield = !requiredby ? &res->dependencies.ndepends : &res->dependencies.nrequiredby ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    stralloc sa = STRALLOC_ZERO ;
    char const *exclude[4] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, SS_MODULE_CONFIG_DIR + 1, 0 } ;

    info->opt_tree = 0 ;

    FOREACH_SASTR(list, pos) {

        sa.len = 0 ;
        char fname[strlen(list->s + pos)] ;

        if (!ob_basename(fname, list->s + pos))
            log_dieusys(LOG_EXIT_ZERO, "basename of: ", list->s + pos) ;

        /** cannot call itself */
        if (!strcmp(name, fname))
            log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

        if (!service_frontend_path(&sa, fname, info->owner, 0, exclude))
            log_dieu(LOG_EXIT_USER, "find service frontend file of: ", fname) ;

        if (!stack_add_g(&stk, fname))
            log_dieusys(LOG_EXIT_SYS, "handle service dependencies list") ;

        (*nfield)++ ;

        parse_frontend(sa.s, ares, areslen, info, force, conf, 0, fname, 0) ;

    }

    info->opt_tree = opt_tree ;

    if (!stack_close(&stk))
        log_dieusys(LOG_EXIT_SYS, "close stack") ;

    if (!stack_convert_tostring(&stk))
        log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

    if (*nfield) {

        size_t len = strlen(res->sa.s + *field) ;
        char tmp[len + stk.len + 2] ;
        auto_strings(tmp, res->sa.s + *field, " ", stk.s) ;
        (*field) = resolve_add_string(wres, tmp) ;

    } else {

        (*field) = resolve_add_string(wres, stk.s) ;
    }

    stralloc_free(&sa) ;
    free(wres) ;
}

static void parse_module_regex(resolve_service_t *res, char *copy, size_t copylen, ssexec_t *info)
{
    char *name = res->sa.s + res->name ;
    stralloc list = STRALLOC_ZERO ;

    /** contents */
    {
        auto_strings(copy + copylen, SS_MODULE_FRONTEND) ;

        char const *exclude[1] = { 0 } ;

        get_list(&list, copy, name, S_IFREG, exclude) ;
        regex_replace(&list, res) ;
    }

    {
        copy[copylen] = 0 ;

        char const *exclude[4] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, SS_MODULE_CONFIG_DIR + 1, 0 } ;

        /** directories */
        get_list(&list, copy, name, S_IFDIR, exclude) ;
        regex_rename(&list, res, res->regex.directories) ;

        /** filename */
        get_list(&list, copy, name, S_IFREG, exclude) ;
        regex_rename(&list, res, res->regex.files) ;
    }

    stralloc_free(&list) ;

    /** configure script */
    regex_configure(res, info, copy, name) ;
}

void parse_module(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force)
{
    log_flow() ;

    int r, insta = -1 ;
    size_t pos = 0, copylen = 0, len = 0 ;
    uint8_t opt_tree = info->opt_tree ;
    char name[strlen(res->sa.s + res->name) + 1] ;
    char src[strlen(res->sa.s + res->path.frontend) + 1] ;
    char dirname[strlen(src) + 1] ;
    char copy[SS_MAX_PATH_LEN + 1] ;
    char ainsta[strlen(res->sa.s + res->name) + 1] ;
    stralloc list = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    auto_strings(name,res->sa.s + res->name) ;
    auto_strings(src,res->sa.s + res->path.frontend) ;

    log_trace("parse module: ", name) ;

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!ob_dirname(dirname, src))
        log_dieu(LOG_EXIT_SYS, "get directory name of: ", src) ;

    insta = instance_check(name) ;
    instance_splitname_to_char(ainsta, name, insta, 0) ;

    if (!getuid()) {

        auto_strings(copy, SS_SERVICE_ADMDIR, name) ;

        copylen = strlen(copy) ;

    } else {

        if (!set_ownerhome_stack(copy))
            log_dieusys(LOG_EXIT_SYS, "unable to find the home directory of the user") ;

        copylen = strlen(copy) ;

        auto_strings(copy + copylen, SS_SERVICE_USERDIR, name) ;
    }

    uint8_t conf = res->environ.env_overwrite ;

    /** check mandatory directories */
    parse_module_check_dir(dirname, SS_MODULE_CONFIG_DIR) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
    parse_module_check_dir(dirname, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
    parse_module_check_dir(dirname, SS_MODULE_FRONTEND) ;

    r = scan_mode(copy, S_IFDIR) ;
    if (r == -1) {
        errno = EEXIST ;
        log_dieusys(LOG_EXIT_SYS, "conflicting format of: ", copy) ;

    } else {

        if (force || !r) {

            if (r) {
                log_trace("remove directory: ", copy) ;
                if (!dir_rm_rf(copy))
                    log_dieusys (LOG_EXIT_SYS, "remove: ", copy) ;
            }

            log_trace("copy: ", dirname, " to: ", copy) ;
            if (!hiercopy(dirname, copy))
                log_dieusys(LOG_EXIT_SYS, "copy: ", dirname, " to: ", copy) ;

            auto_strings(copy + copylen, "/", ainsta) ;

            /** remove the original service frontend file inside the copied directory
             * to avoid double frontend service file for a same service.*/
            errno = 0 ;
            if (unlink(copy) < 0 && errno != ENOENT)
                log_dieusys(LOG_EXIT_ZERO, "unlink: ", copy) ;

            copy[copylen] = 0 ;

            parse_module_regex(res, copy, copylen, info) ;

        } else
            log_warn("skip configuration of the module: ", name, " -- already configured") ;
    }

    /** handle new activated depends/requiredby service.*/
    {
        char const *exclude[1] = { 0 } ;
        auto_strings(copy + copylen, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS) ;
        get_list(&list, copy, name, S_IFREG, exclude) ;

        parse_module_dependencies(&list, res, 0, ares, areslen, force, conf, info) ;

        auto_strings(copy + copylen, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY) ;
        get_list(&list, copy, name, S_IFREG, exclude) ;

        parse_module_dependencies(&list, res, 1, ares, areslen, force, conf, info) ;
    }

    auto_strings(copy + copylen, SS_MODULE_ACTIVATED) ;

    {
        char const *exclude[3] = { SS_MODULE_DEPENDS + 1, SS_MODULE_REQUIREDBY + 1, 0 } ;

        get_list(&list, copy, name, S_IFREG, exclude) ;
    }

    auto_strings(copy + copylen, SS_MODULE_FRONTEND) ;

    {
        /* parse each activated services */
        len = list.len ;
        char l[len + 1] ;
        char ebase[copylen + 1] ;
        memcpy(ebase, copy, copylen) ;
        ebase[copylen] = 0 ;

        if (!ob_basename(dirname, dirname))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        if (!ob_basename(ebase, ebase))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        sastr_to_char(l, &list) ;

        list.len = 0 ;

        for (pos = 0 ; pos < len ; pos += strlen(l + pos) + 1) {

            list.len = 0 ;
            char fname[strlen(l + pos)] ;
            char const *exclude[1] = { 0 } ;

            if (!ob_basename(fname, l + pos))
                log_dieusys(LOG_EXIT_ZERO, "basename of: ", l + pos) ;

            /** cannot call itself */
            if (!strcmp(name, fname))
                log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

            /** Search first inside the module directory.
             * If not found, warn user about what to do.*/
            if (!service_frontend_path(&list, fname, info->owner, copy, exclude)) {

                copy[copylen] = 0 ;
                char deps[copylen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_DEPENDS_LEN + 1 + strlen(fname) + 1] ;
                char require[copylen + SS_MODULE_ACTIVATED_LEN + SS_MODULE_REQUIREDBY_LEN + 1 + strlen(fname) + 1] ;

                auto_strings(deps, copy, SS_MODULE_ACTIVATED SS_MODULE_DEPENDS, "/", fname) ;
                auto_strings(require, copy, SS_MODULE_ACTIVATED SS_MODULE_REQUIREDBY, "/", fname) ;
                log_die(LOG_EXIT_USER, "you can not activate the service ", fname, " without providing its frontend file at ",copy, \
                                    ". If you want to add an depends/requiredby service to the module, consider creating a named empty file at ", \
                                    deps, " or ", require) ;

            }

            info->opt_tree = 1 ;
            info->treename.len = 0 ;
            if (!auto_stra(&info->treename, res->sa.s + res->treename))
                log_die_nomem("stralloc") ;

            parse_frontend(list.s, ares, areslen, info, force, conf, copy, fname, name) ;

            info->opt_tree = opt_tree ;
        }

    }

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

    /** append the module name at each inner depends/requiredby dependencies service name
     * and define contents field.*/
    parse_rename_interdependences(res, name, ares, areslen) ;

    free(wres) ;
    stralloc_free(&list) ;
}
