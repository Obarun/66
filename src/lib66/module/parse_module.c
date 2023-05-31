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

static void parse_module_prefix(char *result, stralloc *sa, resolve_service_t *ares, unsigned int areslen, char const *module)
{
    log_flow() ;

    int aresid = -1 ;
    size_t pos = 0, mlen = strlen(module) ;

    FOREACH_SASTR(sa, pos) {

        aresid = service_resolve_array_search(ares, areslen, sa->s + pos) ;

        if (aresid < 0) {
            /** try with the name of the module as prefix */
            char tmp[mlen + 1 + strlen(sa->s + pos) + 1] ;

            auto_strings(tmp, module, ":", sa->s + pos) ;

            aresid = service_resolve_array_search(ares, areslen, tmp) ;
            if (aresid < 0)
                log_die(LOG_EXIT_USER, "service: ", sa->s + pos, " not available -- please make a bug report") ;
        }

        /** check if the dependencies is a external one. In this
         * case, the service is not considered as part of the module */
        if (ares[aresid].inmodule && (!strcmp(ares[aresid].sa.s + ares[aresid].inmodule, module)))
            auto_strings(result + strlen(result), module, ":", sa->s + pos, " ") ;
        else
            auto_strings(result + strlen(result), sa->s + pos, " ") ;
    }

    result[strlen(result) - 1] = 0 ;
}

static void parse_convert_tomodule(unsigned int idx, resolve_service_t *ares, unsigned int areslen, char const *module)
{
    log_flow() ;

    size_t mlen = strlen(module) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &ares[idx]) ;
    stralloc sa = STRALLOC_ZERO ;

    if (ares[idx].dependencies.ndepends) {

        if (!sastr_clean_string(&sa, ares[idx].sa.s + ares[idx].dependencies.depends))
            log_die_nomem("stralloc") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * ares[idx].dependencies.ndepends ;
        char n[len] ;

        memset(n, 0, len) ;

        parse_module_prefix(n, &sa, ares, areslen, module) ;

        ares[idx].dependencies.depends = resolve_add_string(wres, n) ;

    }

    if (ares[idx].dependencies.nrequiredby) {

        sa.len = 0 ;

        if (!sastr_clean_string(&sa, ares[idx].sa.s + ares[idx].dependencies.requiredby))
            log_die_nomem("stralloc") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * ares[idx].dependencies.nrequiredby ;
        char n[len] ;

        memset(n, 0, len) ;

        parse_module_prefix(n, &sa, ares, areslen, module) ;

        ares[idx].dependencies.requiredby = resolve_add_string(wres, n) ;

    }

    stralloc_free(&sa) ;
    free(wres) ;
}

void parse_module(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force)
{
    log_flow() ;

    int r, insta = -1 ;
    size_t pos = 0, copylen = 0, len = 0 ;
    char name[strlen(res->sa.s + res->name) + 1] ;
    auto_strings(name,res->sa.s + res->name) ;
    char *src = res->sa.s + res->path.frontend ;
    char dirname[strlen(src)] ;
    char copy[SS_MAX_PATH_LEN] ;
    char ainsta[strlen(name)] ;
    stralloc list = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

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
    parse_module_check_dir(dirname, SS_MODULE_FRONTEND) ;

    r = scan_mode(copy, S_IFDIR) ;
    if (r == -1) {
        errno = EEXIST ;
        log_dieusys(LOG_EXIT_SYS, "conflicting format of: ", copy) ;

    } else if (!r) {

        log_trace("copy: ", dirname, " to: ", copy) ;
        if (!hiercopy(dirname, copy))
            log_dieusys(LOG_EXIT_SYS, "copy: ", dirname, " to: ", copy) ;

    } else {

        /** Must reconfigure all services of the module */
        if (!force) {

            log_warn("skip configuration of the module: ", name, " -- already configured") ;
            goto deps ;
        }

        log_trace("remove directory: ", copy) ;
        if (!dir_rm_rf(copy))
            log_dieusys (LOG_EXIT_SYS, "remove: ", copy) ;

        log_trace("copy: ", dirname, " to: ", copy) ;
        if (!hiercopy(dirname, copy))
            log_dieusys(LOG_EXIT_SYS,"copy: ", dirname, " to: ", copy) ;
    }

    auto_strings(copy + copylen, "/", ainsta) ;

    /** remove the original service frontend file inside the copied directory
     * to avoid double frontend service file for a same service.*/
    errno = 0 ;
    if (unlink(copy) < 0 && errno != ENOENT)
        log_dieusys(LOG_EXIT_ZERO, "unlink: ", copy) ;

    copy[copylen] = 0 ;

    auto_strings(copy + copylen, SS_MODULE_FRONTEND) ;

    /** contents */
    get_list(&list, copy, name, S_IFREG) ;
    regex_replace(&list, res) ;

    copy[copylen] = 0 ;

    /** directories */
    get_list(&list, copy, name, S_IFDIR) ;
    regex_rename(&list, res, res->regex.directories) ;

    /** filename */
    get_list(&list, copy, name, S_IFREG) ;
    regex_rename(&list, res, res->regex.files) ;

    /** configure script */
    regex_configure(res, info, copy, name) ;

    deps:

    auto_strings(copy + copylen, SS_MODULE_ACTIVATED) ;

    get_list(&list, copy, name, S_IFREG) ;

    auto_strings(copy + copylen, SS_MODULE_FRONTEND) ;

    {
        /* parse each activated services */
        len = list.len ;
        uint8_t out = 0 ;
        char l[len + 1] ;
        stralloc sa = STRALLOC_ZERO ;

        sastr_to_char(l, &list) ;

        list.len = 0 ;

        for (pos = 0 ; pos < len ; pos += strlen(l + pos) + 1) {

            sa.len = 0 ;
            out = 0 ;
            char fname[strlen(l + pos)] ;

            if (!ob_basename(fname, l + pos))
                log_dieusys(LOG_EXIT_ZERO, "basename of: ", l + pos) ;

            /** cannot call itself */
            if (!strcmp(name, fname))
                log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

            /** Search first inside the frontend directory.
             * If not found, search in the entire system. */
            if (!service_frontend_path(&sa, fname, info->owner, copy)) {

                if (!service_frontend_path(&sa, fname, info->owner, 0))
                    log_dieu(LOG_EXIT_USER, "find service frontend file of: ", fname) ;

                out++;
            }

            /* The module has no knowledge of services outside of it,
             * and the system has no knowledge of services inside the module.
             * If a service frontend is not defined inside the module,
             * consider it as a typical dependency of the module itself
             * and not as part of the module. */
            if (!out) {

                char n[strlen(name) + 1 + strlen(fname) + 1] ;

                auto_strings(n, name, ":", fname) ;

                if (!sastr_add_string(&list, n))
                    log_die_nomem("stralloc") ;

            } else {


                if (res->dependencies.ndepends) {

                    size_t len = strlen(res->sa.s + res->dependencies.depends) ;
                    char tmp[len + strlen(fname) + 2] ;
                    auto_strings(tmp, res->sa.s + res->dependencies.depends, " ", fname) ;
                    res->dependencies.depends = resolve_add_string(wres, tmp) ;

                } else {

                    res->dependencies.depends = resolve_add_string(wres, fname) ;
                }

                res->dependencies.ndepends++ ;
            }

            parse_frontend(sa.s, ares, areslen, info, force, conf, !out ? copy : 0, fname, !out ? name : 0) ;
        }

        stralloc_free(&sa) ;
    }

    {
        /* make a good list of dependencies/requiredby*/
        len = list.len ;

        char l[len + 1] ;

        sastr_to_char(l, &list) ;

        list.len = 0 ;

        for (pos = 0 ; pos < len ; pos += strlen(l + pos) + 1) {

            int aresid = service_resolve_array_search(ares, *areslen, l + pos) ;
            if (aresid < 0)
                log_die(LOG_EXIT_USER, "service: ", l + pos, " not available -- please make a bug report") ;

            if (ares[aresid].dependencies.ndepends || ares[aresid].dependencies.nrequiredby)
                parse_convert_tomodule(aresid, ares, *areslen, name) ;

            if (ares[aresid].logger.want && ares[aresid].type == TYPE_CLASSIC) {

                char n[strlen(ares[aresid].sa.s + ares[aresid].name) + SS_LOG_SUFFIX_LEN + 1] ;

                auto_strings(n, ares[aresid].sa.s + ares[aresid].name, SS_LOG_SUFFIX) ;

                if (!sastr_add_string(&list, n))
                    log_die_nomem("stralloc") ;
            }

            if (!sastr_add_string(&list, l + pos))
                log_die_nomem("stralloc") ;
        }
    }

    res->dependencies.contents = parse_compute_list(wres, &list, &res->dependencies.ncontents, 0) ;

    free(wres) ;
    stralloc_free(&list) ;
}
