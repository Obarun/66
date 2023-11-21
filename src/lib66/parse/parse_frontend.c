/*
 * parse_frontend.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h> //free
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/environ.h>
#include <66/enum.h>
#include <66/state.h> // service_is_g flag
#include <66/parse.h>
#include <66/module.h>
#include <66/instance.h>
#include <66/sanitize.h>

static void parse_service_instance(stralloc *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    char const *exclude[1] = { 0 } ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        log_die(LOG_EXIT_SYS, "split instance service of: ", sv) ;

    log_trace("read frontend service at: ", svsrc, sa.s) ;

    if (read_svfile(frontend, sa.s, svsrc) <= 0) {

        char instaname[sa.len + 1] ;
        auto_strings(instaname, sa.s) ;
        sa.len = 0 ;
        /** in module the template service may not exist e.g.
         * module which call another module. In this case
         * follow the classic way */
        int r = service_frontend_path(&sa, sv, getuid(), 0, exclude) ;
        if (r < 1)
            log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", sv) ;

        char svsrc[sa.len + 1] ;

        if (!ob_dirname(svsrc, sa.s))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", sa.s) ;

        if (read_svfile(frontend, instaname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service at: ", svsrc, instaname) ;
    }

    stralloc_free(&sa) ;

    if (!instance_create(frontend, sv, SS_INSTANCE_REGEX, insta))
        log_die(LOG_EXIT_SYS, "create instance service: ", sv) ;

}

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Die on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */
int parse_frontend(char const *sv, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns)
{
    log_flow() ;

    int insta, isparsed ;
    uint8_t opt_tree_forced = 0 ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1] ;
    stralloc sa = STRALLOC_ZERO ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    if (inns) {

        if (service_resolve_array_search(ares, *areslen, svname) >= 0)
            log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns, ":", svname) ;

        if (service_resolve_array_search(ares, *areslen, n) >= 0)
            log_warn_return(2, "ignoring: ", n, " service -- already appended to the selection") ;

    } else {

        if (service_resolve_array_search(ares, *areslen, svname) >= 0)
            log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;
    }

    log_trace("parse service: ", sv) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        parse_service_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service at: ", sv) ;

        if (read_svfile(&sa, svname, svsrc) <= 0)
            log_dieu(LOG_EXIT_SYS, "read frontend service at: ", sv) ;
    }

    char file[sa.len + 1] ;
    auto_strings(file, sa.s) ;

    isparsed = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (isparsed == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;
    else if (!isparsed)
        isparsed = STATE_FLAGS_FALSE ;

    if (isparsed == STATE_FLAGS_TRUE && !force)
        log_warn_return(2, "ignoring service: ", svname, " -- already parsed") ;

    {
        /** search for the type field*/
        if (!environ_get_val_of_key(&sa, get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_TYPE)))
            log_dieu(LOG_EXIT_SYS, "get field ", get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_TYPE)," of service: ", svname) ;

        char store[sa.len + 1] ;
        auto_strings(store, sa.s) ;

        if (!parse_store_main(&res, store, SECTION_MAIN, KEY_MAIN_TYPE))
            log_dieu(LOG_EXIT_SYS, "store field type of service: ", svname) ;

        sa.len = 0 ;
        if (!auto_stra(&sa, file))
            log_die_nomem("stralloc") ;
    }

    if (!info->opt_tree) {

        /** search for the intree field.
         * This field is not mandatory, do not crash if it not found */
        if (environ_get_val_of_key(&sa, get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_INTREE))) {

            char store[sa.len + 1] ;
            auto_strings(store, sa.s) ;

            if (!parse_store_main(&res, store, SECTION_MAIN, KEY_MAIN_INTREE))
                log_dieu(LOG_EXIT_SYS, "store field intree of service: ", svname) ;

            info->treename.len = 0 ;
            info->opt_tree = 1 ;
            opt_tree_forced = 1 ;
            sa.len = 0 ;

            if (!auto_stra(&info->treename, res.sa.s + res.intree) ||
                !auto_stra(&sa, file))
                    log_die_nomem("stralloc") ;
        }
    }

    if (inns) {

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns,":",svname) ;

        res.name = resolve_add_string(wres, n) ;
        res.inns = resolve_add_string(wres, inns) ;

    } else {

        res.name = resolve_add_string(wres, svname) ;
    }

    res.owner = info->owner ;
    res.ownerstr = resolve_add_string(wres, info->ownerstr) ;
    res.path.home = resolve_add_string(wres, info->base.s) ;
    res.path.frontend = resolve_add_string(wres, sv) ;

    /** logger is set by default. if service is a module
     * we don't want logger. So, set it false by default. */
    if (res.type == TYPE_MODULE)
        res.logger.want = 0 ;

    // keep overwrite_conf
    res.environ.env_overwrite = conf ;

    /** contents of directory should be listed by service_frontend_path
     * except for module type */
    if (scan_mode(sv, S_IFDIR) == 1 && res.type != TYPE_MODULE) {
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return 1 ;
    }

    if (!parse_contents(wres, file, svname))
        log_dieu(LOG_EXIT_SYS, "parse file of service: ", svname) ;

    if (!parse_mandatory(&res))
        log_die(LOG_EXIT_SYS, "some mandatory field is missing for service: ", svname) ;

    /** try to create the tree if not exist yet with
     * the help of the seed files */
    set_treeinfo(info) ;

    res.treename = resolve_add_string(wres, info->treename.s) ;

    if (opt_tree_forced)
        info->opt_tree = 0 ;

    /** append res.dependencies.depends list with the optional dependencies list */
    if (res.dependencies.noptsdeps) {

        if (res.dependencies.ndepends) {
            size_t len = strlen(res.sa.s + res.dependencies.depends) ;
            char t[len + strlen(res.sa.s + res.dependencies.optsdeps) + 2] ;
            auto_strings(t,res.sa.s + res.dependencies.depends, " ", res.sa.s + res.dependencies.optsdeps) ;
            res.dependencies.depends = resolve_add_string(wres, t) ;

        } else {

            res.dependencies.depends = resolve_add_string(wres, res.sa.s + res.dependencies.optsdeps) ;
        }
        res.dependencies.ndepends += res.dependencies.noptsdeps ;
    }

    /** parse interdependences if the service was never parsed */
    if (isparsed == STATE_FLAGS_FALSE) {

        if (!parse_interdependences(svname, res.sa.s + res.dependencies.depends, res.dependencies.ndepends, ares, areslen, info, force, conf, forced_directory, main, inns))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", svname) ;
    }

    if (res.type == TYPE_MODULE)
        parse_module(&res, ares, areslen, info, force) ;

    if (service_resolve_array_search(ares, *areslen, res.sa.s + res.name) < 0) {

        log_trace("add service ", res.sa.s + res.name, " to the selection") ;
        if (*areslen > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;
        ares[(*areslen)++] = res ;
    }

    stralloc_free(&sa) ;
    free(wres) ;
    return 1 ;
}

