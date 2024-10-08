/*
 * parse_frontend.c
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

static void parse_read_instance(stralloc *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    uint8_t exlen = 0 ; // see service_frontend_path file and compute_exclude()
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
        int r = service_frontend_path(&sa, sv, getuid(), 0, exclude, exlen) ;
        if (r < 1)
            log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", sv) ;

        char svsrc[sa.len + 1] ;

        if (!ob_dirname(svsrc, sa.s))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", sa.s) ;

        if (read_svfile(frontend, instaname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service at: ", svsrc, instaname) ;
    }

    stralloc_free(&sa) ;
}

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Die on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */

int parse_frontend(char const *sv,
                   struct resolve_hash_s **hres,
                   ssexec_t *info,
                   uint8_t force,
                   uint8_t conf,
                   char const *forced_directory,
                   char const *main,
                   char const *inns,
                   char const *intree,
                   resolve_service_t *moduleres)
{
    log_flow() ;

    int insta, isparsed ;
    uint8_t opt_tree_forced = 0 ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1], instaname[svlen + 1] ;
    stralloc sa = STRALLOC_ZERO ;
    struct resolve_hash_s *hash ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    if (inns) {

        hash = hash_search(hres, svname) ;
        if (hash != NULL)
            log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns, ":", svname) ;

        hash = hash_search(hres, n) ;
        if (hash != NULL)
            log_warn_return(2, "ignoring: ", n, " service -- already appended to the selection") ;

    } else {

        hash = hash_search(hres, svname) ;
        if (hash != NULL)
            log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;
    }

    log_trace("parse service: ", sv) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        auto_strings(instaname, svname) ;
        instaname[insta + 1] = 0 ;

        parse_read_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service at: ", sv) ;

        if (read_svfile(&sa, svname, svsrc) <= 0)
            log_dieu(LOG_EXIT_SYS, "read frontend service at: ", sv) ;
    }

    if (!identifier_replace(&sa, svname))
        log_dieu(LOG_EXIT_SYS, "replace regex for service: ", svname) ;

    _alloc_stk_(store, sa.len + 1) ;

    isparsed = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (isparsed == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;
    else if (!isparsed)
        isparsed = STATE_FLAGS_FALSE ;

    if (isparsed == STATE_FLAGS_TRUE && !force)
        log_warn_return(2, "ignoring service: ", svname, " -- already parsed") ;

    {
        if (!parse_get_value_of_key(&store, sa.s, SECTION_MAIN, list_section_main, KEY_MAIN_TYPE))
            log_dieu(LOG_EXIT_SYS, "get field ", get_key_by_enum(list_section_main, KEY_MAIN_TYPE)," of service: ", svname) ;

        if (!stack_close(&store))
            log_die_nomem("stack overflow") ;

        if (!parse_store_main(&res, &store, SECTION_MAIN, KEY_MAIN_TYPE))
            log_dieu(LOG_EXIT_SYS, "store field type of service: ", svname) ;
    }

    if (!info->opt_tree) {

        stack_reset(&store) ;

        /** search for the intree field.
         * This field is not mandatory, do not crash if it not found */
        if (parse_get_value_of_key(&store, sa.s, SECTION_MAIN, list_section_main, KEY_MAIN_INTREE)) {

            if (!parse_store_main(&res, &store, SECTION_MAIN, KEY_MAIN_INTREE))
                log_dieu(LOG_EXIT_SYS, "store field intree of service: ", svname) ;

            info->treename.len = 0 ;
            info->opt_tree = 1 ;
            opt_tree_forced = 1 ;

            if (!auto_stra(&info->treename, res.sa.s + res.intree))
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

    {
        char *realname = 0 ;
        if (insta > 0) {
            realname = instaname ;
        } else {
            realname = svname ;
        }

        if (inns) {

            char *path = moduleres->sa.s + moduleres->path.frontend ;
            size_t mlen = strlen(path) ;
            _alloc_stk_(dir, mlen) ;

            if (!ob_dirname(dir.s, path))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", path) ;

            _alloc_stk_(frontend, mlen + SS_MODULE_FRONTEND_LEN + strlen(realname) + 2) ;

            auto_strings(frontend.s, dir.s, SS_MODULE_FRONTEND + 1, "/", realname) ;

            res.path.frontend = resolve_add_string(wres, frontend.s) ;

        } else {

            _alloc_stk_(frontend, svlen + 1) ;

            auto_strings(frontend.s, svsrc, realname) ;

            res.path.frontend = resolve_add_string(wres, frontend.s) ;
        }
    }

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

    if (!parse_contents(&res, sa.s))
        log_dieu(LOG_EXIT_SYS, "parse file of service: ", svname) ;

    if (!parse_mandatory(&res, info))
        log_die(LOG_EXIT_SYS, "some mandatory field is missing for service: ", svname) ;

    /** try to create the tree if not exist yet with
     * the help of the seed files */
    set_treeinfo(info) ;

    res.treename = resolve_add_string(wres, info->treename.s) ;

    if (inns && intree)
        res.intree = resolve_add_string(wres, intree) ;

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

        if (!parse_interdependences(svname, res.sa.s + res.dependencies.depends, res.dependencies.ndepends, hres, info, force, conf, forced_directory, main, inns, intree, moduleres))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", svname) ;
    }

    if (res.type == TYPE_MODULE)
        parse_module(&res, hres, info, force) ;

    parse_compute_resolve(&res, info) ;

    if ((res.logger.want && res.io.fdin.type == IO_TYPE_S6LOG) &&
        (!res.inns && res.type != TYPE_MODULE))
            parse_create_logger(hres, &res, info) ;

    hash = hash_search(hres, res.sa.s + res.name) ;
    if (hash == NULL) {

        if (hash_count(hres) > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

        log_trace("add service: ", res.sa.s + res.name, " to the service selection") ;
        char *name = res.sa.s + res.name ; // hash_add + log_dieu doesn't accept res.sa.s + res.name
        if (!hash_add(hres, name, res))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", name) ;
    }

    stralloc_free(&sa) ;
    free(wres) ;
    return 1 ;
}

