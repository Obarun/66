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
#include <66/parser.h>

void parse_module(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force)
{
    log_flow() ;

    int r, insta = -1 ;
    unsigned int residx = 0 ;
    size_t pos = 0, pathlen = 0 ;
    char *name = res->sa.s + res->name ;
    char *src = res->sa.s + res->path.frontend ;
    char dirname[strlen(src) + 1] ;
    char path[SS_MAX_PATH_LEN] ;
    char ainsta[strlen(name)] ;
    stralloc list = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    log_trace("parse module: ", name) ;

    if (!ob_dirname(dirname, src))
        log_dieu(LOG_EXIT_SYS, "get directory name of: ", src) ;

    insta = instance_check(name) ;
    instance_splitname_to_char(ainsta, name, insta, 0) ;

    size_t prefixlen = strlen(name) ;

    char prefix[prefixlen + 2] ;

    instance_splitname_to_char(prefix, name, insta, 1) ;

    auto_strings(prefix + strlen(prefix), "-") ;
    prefixlen = strlen(prefix) ;

    if (!getuid()) {

        auto_strings(path, SS_SERVICE_ADMDIR, name) ;

    } else {

        if (!set_ownerhome_stack(path))
            log_dieusys(LOG_EXIT_SYS, "unable to find the home directory of the user") ;

        pathlen = strlen(path) ;
        auto_strings(path + pathlen, SS_SERVICE_USERDIR, name) ;
    }

    uint8_t conf = res->environ.env_overwrite ;

    /** check mandatory directories
     * res->frontend/module_name/{configure,service,service@} */
    parse_module_check_dir(dirname, SS_MODULE_CONFIG_DIR) ;
    parse_module_check_dir(dirname, SS_MODULE_SERVICE) ;
    parse_module_check_dir(dirname, SS_MODULE_SERVICE_INSTANCE) ;

    r = scan_mode(path, S_IFDIR) ;
    if (r == -1) {
        errno = EEXIST ;
        log_dieusys(LOG_EXIT_SYS, "conflicting format of: ", path) ;

    } else if (!r) {

        if (!hiercopy(dirname, path))
            log_dieusys(LOG_EXIT_SYS, "copy: ", dirname, " to: ", path) ;

    } else {

        /** Must reconfigure all services of the module */
        if (force < 1) {

            log_warn("skip configuration of the module: ", name, " -- already configured") ;
            goto deps ;
        }

        if (dir_rm_rf(path) < 0)
            log_dieusys (LOG_EXIT_SYS, "remove: ", path) ;

        if (!hiercopy(dirname, path))
            log_dieusys(LOG_EXIT_SYS,"copy: ", dirname, " to: ", path) ;
    }

    pathlen = strlen(path) ;
    /** remove the original service frontend file inside the copied directory
     * to avoid double frontend service file for a same service.*/
    auto_strings(path + pathlen, "/", ainsta) ;

    if (unlink(path) < 0)
        log_dieusys(LOG_EXIT_ZERO, "unlink: ", path) ;

    path[pathlen] = 0 ;

    /** contents */
    get_list(&list, path, name, S_IFREG) ;
    regex_replace(&list, res) ;

    /** directories */
    get_list(&list, path, name, S_IFDIR) ;
    regex_rename(&list, res, res->regex.directories) ;

    /** filename */
    get_list(&list, path, name, S_IFREG) ;
    regex_rename(&list, res, res->regex.files) ;

    /** configure script */
    regex_configure(res, info, path, name) ;

    deps:

    list.len = 0 ;

    if (!auto_stra(&list, path))
        log_die_nomem("stralloc") ;

    char t[list.len] ;

    sastr_to_char(t, &list) ;

    list.len = 0 ;
    char const *exclude[3] = { SS_MODULE_CONFIG_DIR + 1, SS_MODULE_SERVICE_INSTANCE + 1, 0 } ;
    if (!sastr_dir_get_recursive(&list, t, exclude, S_IFREG, 1))
        log_dieusys(LOG_EXIT_SYS, "get file(s) of module: ", name) ;

    char ll[list.len] ;
    size_t llen = list.len ;

    sastr_to_char(ll, &list) ;

    list.len = 0 ;

    for (pos = 0 ; pos < llen ; pos += strlen(ll + pos) + 1) {

        char *dname = ll + pos ;
        char ainsta[pathlen + SS_MODULE_SERVICE_INSTANCE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        size_t namelen = strlen(dname) ;
        char bname[namelen] ;

        if (!ob_basename(bname,dname))
            log_dieu(LOG_EXIT_SYS, "find basename of: ", dname) ;

        if (instance_check(bname) > 0) {
            auto_strings(ainsta, path, SS_MODULE_SERVICE_INSTANCE, "/", bname) ;
            dname = ainsta ;
        }

        if (!sastr_add_string(&list, bname))
            log_die_nomem("stralloc") ;

        if (!strcmp(name, bname))
            log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", bname) ;

        /** nothing to do with the exit code*/
        parse_frontend(dname, ares, areslen, info, force, conf, &residx, path, bname) ;

        wres = resolve_set_struct(DATA_SERVICE, &ares[residx]) ;

        ares[residx].inmodule = resolve_add_string(wres, name) ;
    }

    char deps[list.len] ;

    sastr_to_char(deps, &list) ;

    llen = list.len ;

    list.len = 0 ;

    /* rebuild the dependencies list incorporating the services defined inside
     * the module.

    if (res->dependencies.ndepends)
        if (!sastr_clean_string(&list, res->sa.s + res->dependencies.depends))
            log_dieu(LOG_EXIT_SYS, "clean string") ;



    res->dependencies.depends = parse_compute_list(wres, &list, &res->dependencies.ndepends, 0) ;

    list.len = 0 ;
*/

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    for (pos = 0 ; pos < llen ; pos += strlen(deps + pos) + 1)
        if (!sastr_add_string(&list, deps + pos))
            log_die_nomem("stralloc") ;

    res->regex.contents = parse_compute_list(wres, &list, &res->regex.ncontents, 0) ;

    free(wres) ;
    stralloc_free(&list) ;
}
