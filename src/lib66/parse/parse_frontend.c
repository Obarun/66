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
#include <66/parser.h>

static void parse_service_instance(stralloc *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        log_die(LOG_EXIT_SYS, "split instance service of: ", sv) ;

    log_trace("read frontend service of: ", svsrc, sv) ;

    if (read_svfile(frontend, sa.s, svsrc) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read frontend service of: ", svsrc, sa.s) ;

    stralloc_free(&sa) ;

    if (!instance_create(frontend, sv, SS_INSTANCE_REGEX, insta))
        log_die(LOG_EXIT_SYS, "create instance service: ", sv) ;

}

static void set_info(ssexec_t *info)
{
    log_flow() ;

    info->tree.len = 0 ;
    int r = tree_sethome(info) ;
    if (r == -3)
        log_dieu(LOG_EXIT_USER, "find the current tree. You must use the -t options") ;
    else if (r == -2)
        log_dieu(LOG_EXIT_USER, "set the tree name") ;
    else if (r == -1)
        log_dieu(LOG_EXIT_USER, "parse seed file") ;
    else if (!r)
        log_dieusys(LOG_EXIT_SYS, "find tree: ", info->treename.s) ;

    if (!tree_get_permissions(info->tree.s, info->owner))
       log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", info->tree.s) ;

    info->treeallow = 1 ;

}

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Die on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */
int parse_frontend(char const *sv, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force, uint8_t conf, unsigned int *residx, char const *forced_directory, char const *main)
{
    log_flow() ;

    int insta, r, isparsed ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1] ;
    char atree[SS_MAX_TREENAME + 1] ;
    stralloc sa = STRALLOC_ZERO ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    if (service_resolve_array_search(ares, *areslen, svname) >= 0)
        log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;

    log_trace("parse service: ", sv) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        parse_service_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service of: ", sv) ;

        if (read_svfile(&sa, svname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service of: ", sv) ;
    }

    char file[sa.len + 1] ;
    auto_strings(file, sa.s) ;

    isparsed = service_is_g(atree, svname, STATE_FLAGS_ISPARSED) ;
    if (isparsed == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;

    if (isparsed) {

        if (!force) {

            log_warn("ignoring service: ", svname, " -- already present at tree: ", atree) ;
            return 2 ;

        } else if (info->opt_tree) {
            /* -t option was used */
            if (strcmp(info->treename.s, atree))
                log_die(LOG_EXIT_SYS,"you can not enable again a service already set on another tree -- current: ", atree, " asked: ", info->treename.s, ". Try first to disable it") ;

        }

    }

    if (info->opt_tree) {

        if (tree_isvalid(info->base.s, info->treename.s) <= 0) {

            /** @intree may not exist */
            r = sastr_find(&sa, get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_INTREE)) ;
            if (r == -1)
                goto follow ;

            if (!environ_get_val_of_key(&sa, get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_INTREE)))
                log_dieu(LOG_EXIT_SYS, "get field intree of service: ", sv) ;

            if (!sastr_clean_element(&sa))
                log_dieu(LOG_EXIT_SYS, "clean field intree of service: ", sv) ;

            res.intree = resolve_add_string(wres, sa.s) ;

            info->treename.len = 0 ;
            if (!auto_stra(&info->treename, res.sa.s + res.intree) ||
                !auto_stra(&sa, file))
                    log_die_nomem("stralloc") ;
        }
    }

    follow:

    if (!environ_get_val_of_key(&sa, get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_TYPE)))
        log_dieu(LOG_EXIT_SYS, "get field ", get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_TYPE)," of service: ", svname) ;

    char store[sa.len + 1] ;
    auto_strings(store, sa.s) ;

    if (!parse_store_main(&res, store, SECTION_MAIN, KEY_MAIN_TYPE))
        log_dieu(LOG_EXIT_SYS, "store field type of service: ", svname) ;

    res.name = resolve_add_string(wres, svname) ;

    res.owner = info->owner ;
    res.ownerstr = resolve_add_string(wres, info->ownerstr) ;

    res.path.frontend = resolve_add_string(wres, sv) ;

    // keep overwrite_conf
    res.environ.env_overwrite = conf ;

    /** try to create the tree if not exist yet with
     * the help of the seed files */
    set_info(info) ;

    /** contents of directory should be listed by service_frontend_path
     * except for module type */
    if (scan_mode(sv,S_IFDIR) == 1 && res.type != TYPE_MODULE)
        goto freed ;

    if (!parse_contents(wres, file, svname))
        log_dieu(LOG_EXIT_SYS, "parse file of service: ", svname) ;

    if (!parse_mandatory(&res))
        log_die(LOG_EXIT_SYS, "some mandatory field is missing for service: ", svname) ;

    /** append res.dependencies.depends list with the optional dependencies list */
    if (res.dependencies.noptsdeps) {

        if (res.dependencies.ndepends) {
            size_t len = strlen(res.sa.s + res.dependencies.depends) ;
            char t[len + strlen(res.sa.s + res.dependencies.optsdeps) + 2] ;
            auto_strings(t + len, " ", res.sa.s + res.dependencies.optsdeps) ;
            res.dependencies.depends = resolve_add_string(wres, t) ;

        } else {

            res.dependencies.depends = resolve_add_string(wres, res.sa.s + res.dependencies.optsdeps) ;
        }
    }

    /** We take the dependencies in two case:
     * If the user ask for it(force > 1)
     * If the service was never parsed(!isparsed)*/
    if (force > 1 || !isparsed)
        if (!parse_dependencies(&res, ares, areslen, info, force, conf, forced_directory, main))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", svname) ;

    parse_compute_resolve(&res, info) ;

    if (res.type == TYPE_MODULE)
        parse_module(&res, ares, areslen, info, force) ;

    log_trace("add service to the selection: ", svname) ;

    if (service_resolve_array_search(ares, *areslen, svname) < 0) {
        if (*areslen >= SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;
        (*residx) = *areslen ;
        ares[(*areslen)++] = res ;
    }

    freed:
        stralloc_free(&sa) ;
        free(wres) ;
        return 1 ;
}
