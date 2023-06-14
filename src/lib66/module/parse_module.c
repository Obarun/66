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
    uint8_t opt_tree = info->opt_tree ;
    char name[strlen(res->sa.s + res->name) + 1] ;
    char *src = res->sa.s + res->path.frontend ;
    char dirname[strlen(src)] ;
    char copy[SS_MAX_PATH_LEN] ;
    char ainsta[strlen(name)] ;
    stralloc list = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    auto_strings(name,res->sa.s + res->name) ;

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

    _init_stack_(stk, list.len + sastr_nelement(&list)) ;
    {
        /* parse each activated services */

        len = list.len ;
        uint8_t out = 0 ;
        char l[len + 1] ;
        char ebase[copylen + 1] ;
        memcpy(ebase, copy, copylen) ;
        ebase[copylen] = 0 ;

        if (!ob_basename(dirname, dirname))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        if (!ob_basename(ebase, ebase))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", dirname) ;

        stralloc sa = STRALLOC_ZERO ;

        sastr_to_char(l, &list) ;

        list.len = 0 ;

        for (pos = 0 ; pos < len ; pos += strlen(l + pos) + 1) {

            sa.len = 0 ;
            out = 0 ;
            info->opt_tree = opt_tree ;
            char fname[strlen(l + pos)] ;
            char const *exclude[1] = { 0 } ;

            if (!ob_basename(fname, l + pos))
                log_dieusys(LOG_EXIT_ZERO, "basename of: ", l + pos) ;

            /** cannot call itself */
            if (!strcmp(name, fname))
                log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", fname) ;

            /** Search first inside the module directory.
             * If not found, search in the entire system. */
            if (!service_frontend_path(&sa, fname, info->owner, copy, exclude)) {

                char const *inside[3] = { ebase, dirname, 0 } ;
                if (!service_frontend_path(&sa, fname, info->owner, 0, inside))
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

                info->opt_tree = 1 ;
                info->treename.len = 0 ;
                if (!auto_stra(&info->treename, res->sa.s + res->intree))
                    log_die_nomem("stralloc") ;

            } else {

                info->opt_tree = 0 ;

                if (!stack_add_g(&stk, fname))
                    log_dieusys(LOG_EXIT_SYS, "handle service dependencies list") ;

                res->dependencies.ndepends++ ;

            }

            parse_frontend(sa.s, ares, areslen, info, force, conf, !out ? copy : 0, fname, !out ? name : 0) ;
        }

        stralloc_free(&sa) ;
    }

    if (stk.len) {

        if (!stack_close(&stk))
            log_dieusys(LOG_EXIT_SYS, "close stack") ;

        if (!stack_convert_tostring(&stk))
            log_dieusys(LOG_EXIT_SYS, "rebuild stack list") ;

        if (res->dependencies.ndepends) {

            size_t len = strlen(res->sa.s + res->dependencies.depends) ;
            char tmp[len + stk.len + 2] ;
            auto_strings(tmp, res->sa.s + res->dependencies.depends, " ", stk.s) ;
            res->dependencies.depends = resolve_add_string(wres, tmp) ;

        } else {

            res->dependencies.depends = resolve_add_string(wres, stk.s) ;
        }
    }

    /** Remove the module name from requiredby field
     * of the dependencies if the service disappears with the
     * fresh parse process.
     *
     * The Module enable the service by the configure script.
     * As long as the frontend come from an external module
     * directory, it will mark the module name as requiredby
     * dependencies at the service.
     *
     * Then the module disable the service. In this case
     * if the requiredby field is not corrected, the sanitize_graph
     * function will found a service which may not exist anymore
     * at the new state of the machine and so, it activate again a removed
     * service from the module.
     *
     * As long as the user asked for the force option, we can retrieve
     * and read the old resolve file (meaning the current one) to
     * compare it with the new one.
     *
     * The problem does not appear in case of first parse process
     * of the module.*/

    if (force) {

        resolve_service_t ores = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref owres = resolve_set_struct(DATA_SERVICE, &ores) ;

        if (resolve_read_g(owres, info->base.s, res->sa.s + res->name) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

        if (ores.dependencies.ndepends) {
            /** open the old resolve file */
            size_t pos = 0, olen = strlen(ores.sa.s + ores.dependencies.depends) ;
            _init_stack_(old, olen + 1) ;

            if (!stack_convert_string(&old, ores.sa.s + ores.dependencies.depends, olen))
                log_dieusys(LOG_EXIT_SYS, "convert string") ;

            {
                resolve_service_t nres = RESOLVE_SERVICE_ZERO ;
                resolve_wrapper_t_ref nwres = resolve_set_struct(DATA_SERVICE, &nres) ;

                size_t clen = strlen(res->sa.s + res->dependencies.depends) ;
                _init_stack_(new, clen + 1) ;

                if (!stack_convert_string(&new, res->sa.s + res->dependencies.depends, clen))
                    log_dieusys(LOG_EXIT_SYS, "convert string") ;

                /** check if the service was deactivated*/
                FOREACH_STK(&old, pos) {

                    if (stack_retrieve_element(&new, old.s + pos) < 0) {

                        char *dname = old.s + pos ;

                        int r = resolve_read_g(nwres, info->base.s, dname) ;
                        if (r < 0)
                            log_die(LOG_EXIT_USER, "read resolve file of: ") ;

                        if (!r)
                            continue ;

                        if (nres.dependencies.nrequiredby) {

                            size_t len = strlen(nres.sa.s + nres.dependencies.requiredby) ;
                            _init_stack_(stk, len + 1) ;

                            if (!stack_convert_string(&stk, nres.sa.s + nres.dependencies.requiredby, len + 1))
                                log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

                            /** remove the module name to the requiredby field of the old service dependency*/
                            if (!stack_remove_element_g(&stk, name))
                                log_dieusys(LOG_EXIT_SYS, "remove element") ;

                            nres.dependencies.nrequiredby = (uint32_t) stack_count_element(&stk) ;

                            if (nres.dependencies.nrequiredby) {
                                if (!stack_convert_tostring(&stk))
                                    log_dieusys(LOG_EXIT_SYS, "convert stack to string") ;

                                nres.dependencies.requiredby = resolve_add_string(nwres, stk.s) ;

                            } else {

                                nres.dependencies.requiredby = resolve_add_string(nwres, "") ;

                                /** If the module was enabled, the service dependency was as well.
                                 * If the service dependency was only activated by the module
                                 * (meaning the service only has the module as a "requiredby" dependency),
                                 * the service should also be disabled.*/

                                {

                                    unsigned int m = 0 ;
                                    int nargc = 4 ;
                                    char const *prog = PROG ;
                                    char const *newargv[nargc] ;

                                    char const *help = info->help ;
                                    char const *usage = info->usage  ;

                                    info->help = help_disable ;
                                    info->usage = usage_disable ;

                                    newargv[m++] = "disable" ;
                                    newargv[m++] = "-P" ;
                                    newargv[m++] = dname ;
                                    newargv[m] = 0 ;

                                    PROG = "disable" ;
                                    if (ssexec_disable(m, newargv, info))
                                        log_dieu(LOG_EXIT_SYS,"disable: ", dname) ;
                                    PROG = prog ;

                                    /** Also, unsupervise it.*/

                                    info->help = help_stop ;
                                    info->usage = usage_stop ;

                                    newargv[0] = "stop" ;
                                    newargv[1] = "-Pu" ;

                                    PROG = "stop" ;
                                    if (ssexec_stop(m, newargv, info))
                                        log_dieu(LOG_EXIT_SYS,"stop: ", dname) ;
                                    PROG = prog ;

                                    info->help = help ;
                                    info->usage = usage ;
                                }
                            }

                            if (!resolve_write_g(nwres, nres.sa.s + nres.path.home, dname))
                                log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", dname) ;
                        }
                    }
                }
                resolve_free(nwres) ;
            }
        }
        resolve_free(owres) ;
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
