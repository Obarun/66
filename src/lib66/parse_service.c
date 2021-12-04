/*
 * parse_service.c
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

#include <66/parser.h>

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/tree.h>

static void parse_service_instance(stralloc *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        log_die(LOG_EXIT_SYS, "split instance service: ", sv) ;

    log_trace("read frontend service of: ", svsrc, sa.s) ;

    if (read_svfile(frontend, sa.s, svsrc) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read frontend service of: ", svsrc, sa.s) ;

    stralloc_free(&sa) ;

    if (!instance_create(frontend, sv, SS_INSTANCE_REGEX, insta))
        log_die(LOG_EXIT_SYS, "create instance service: ", sv) ;

    /** ensure that we have an empty line at the end of the string*/
    if (!auto_stra(frontend, "\n"))
        log_die_nomem("stralloc") ;

}

static int parse_add_service(stralloc *parsed_list, sv_alltype *alltype, char const *service, uint8_t conf)
{
    log_flow() ;

    log_trace("add service: ", service) ;

    // keep overwrite_conf
    alltype->overwrite_conf = conf ;

    // keep source of the frontend file
    alltype->src = keep.len ;

    if (!sastr_add_string(&keep, service))
        return 0 ;

    // keep service on current list
    if (!sastr_add_string(parsed_list, service))
        return 0 ;

    if (!genalloc_append(sv_alltype, &gasv, alltype))
        return 0 ;

    return 1 ;
}

static void set_info(ssexec_t *info)
{
    log_flow() ;

    info->tree.len = 0 ;
    int r = ssexec_set_treeinfo(info) ;
    if (r == -4) log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;
    if (r == -3) log_dieu(LOG_EXIT_USER,"find the current tree. You must use the -t options") ;
    if (r == -2) log_dieu(LOG_EXIT_USER,"set the tree name") ;
    if (r == -1) log_dieu(LOG_EXIT_USER,"parse seed file") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"find tree: ", info->treename.s) ;

}
#include <stdio.h>
/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Return 0 on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */
int parse_service(char const *sv, stralloc *parsed_list, ssexec_t *info, uint8_t force, uint8_t conf)
{

    log_flow() ;

    if (sastr_cmp(parsed_list, sv) >= 0) {

        log_warn("ignoring: ", sv, " service -- already parsed") ;
        return 2 ;
    }

    log_trace("parse service: ", sv) ;

    int insta, r ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1] ;

    sv_alltype alltype = SV_ALLTYPE_ZERO ;
    stralloc frontend = STRALLOC_ZERO ;
    stralloc satree = STRALLOC_ZERO ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    insta = instance_check(svname) ;
    if (!insta) {

        log_die(LOG_EXIT_SYS, "invalid instance name: ", svname) ;

    } else if (insta > 0) {

        parse_service_instance(&frontend, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service of: ", sv) ;

        if (read_svfile(&frontend, svname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service of: ", sv) ;
    }

    r = service_isenabledat(&satree, svname) ;
    if (r < -1)
        log_dieu(LOG_EXIT_SYS, "check already enabled services") ;

    if (r > 0) {

        if (force) {

            /* -t option was used */
            if (!info->skip_opt_tree) {

                if (strcmp(info->treename.s, satree.s))
                    log_die(LOG_EXIT_SYS,"you can not force to enable again a service on different tree -- current: ", satree.s, " asked: ", info->treename.s, ". Try first to disable it") ;

            } else {

                if (!auto_stra(&info->treename, satree.s))
                    log_die_nomem("stralloc") ;
            }

            goto set ;

        } else {

            log_info("ignoring service: ", sv, " -- already enabled at tree: ", satree.s) ;

            /** we don't care about the use of the -t option. The define of the
             * info->tree and info->treename is just made to avoid segmentation fault
             * at the rest of the process. The service is not parsed or enable again anyway. */

            info->treename.len = 0 ;
            if (!auto_stra(&info->treename, satree.s))
                log_die_nomem("stralloc") ;

            set_info(info) ;

            sv_alltype_free(&alltype) ;
            stralloc_free(&frontend) ;
            stralloc_free(&satree) ;
            return 2 ;
        }
    }

    if (info->skip_opt_tree) {

        /** first try to find the @intree key at the frontend file*/
        if (!get_svintree(&alltype,frontend.s))
            log_die(LOG_EXIT_USER, "invalid value for key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_INTREE)," in service file: ", sv) ;

        if (alltype.cname.intree >= 0) {

            info->treename.len = 0 ;
            if (!auto_stra(&info->treename, keep.s + alltype.cname.intree))
                log_die_nomem("stralloc") ;

        }
    }

    set:

    set_info(info) ;

    if (!get_svtype(&alltype, frontend.s))
        log_die(LOG_EXIT_USER, "invalid value for key: ", get_key_by_enum(ENUM_KEY_SECTION_MAIN, KEY_MAIN_TYPE), " at frontend service: ", sv) ;

    /** contents of directory should be listed by service_frontend_path
     * except for module type */
    if (scan_mode(sv,S_IFDIR) == 1 && alltype.cname.itype != TYPE_MODULE)
        goto freed ;

    alltype.cname.name = keep.len ;
    if (!sastr_add_string(&keep, svname))
        log_die_nomem("stralloc") ;

    if (!parser(&alltype, &frontend, svname, alltype.cname.itype))
        log_dieu(LOG_EXIT_SYS, "parse service: ", sv) ;

    if (!parse_add_service(parsed_list, &alltype, sv, conf))
        log_dieu(LOG_EXIT_SYS, "add service: ", sv) ;

    freed:
    stralloc_free(&frontend) ;
    stralloc_free(&satree) ;

    return 1 ;
}

int parse_service_deps(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, char const *directory_forced)
{

    log_flow() ;

    int r, e = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (alltype->cname.nga) {

        size_t id = alltype->cname.idga, nid = alltype->cname.nga ;
        for (; nid ; id += strlen(deps.s + id) + 1, nid--) {

            sa.len = 0 ;

            if (alltype->cname.itype != TYPE_BUNDLE) {

                log_trace("service: ", keep.s + alltype->cname.name, " depends on: ", deps.s + id) ;

            } else log_trace("bundle: ", keep.s + alltype->cname.name, " contents: ", deps.s + id," as service") ;

            r = service_frontend_path(&sa, deps.s + id, info->owner, directory_forced) ;
            if (r < 1) goto err ;//don't warn here, the ss_revolve_src_path do it

            if (!parse_service(sa.s, parsed_list, info, force, alltype->overwrite_conf))
                goto err ;
        }

    } else log_trace(keep.s + alltype->cname.name,": haven't dependencies") ;

    e = 1 ;

    err:
        stralloc_free(&sa) ;
        return e ;
}

int parse_service_optsdeps(stralloc *rebuild, sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, uint8_t field, char const *directory_forced)
{
    log_flow() ;

    int r, e = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    size_t id, nid ;
    uint8_t ext = field == KEY_MAIN_EXTDEPS ? 1 : 0 ;

    int idref = alltype->cname.idopts ;
    unsigned int nref = alltype->cname.nopts ;
    if (ext) {
        idref = alltype->cname.idext ;
        nref = alltype->cname.next ;
    }

    if (nref) {

        id = (size_t)idref, nid = (size_t)nref ;
        for (; nid ; id += strlen(deps.s + id) + 1, nid--) {

            sa.len = 0 ;
            // 0 -> not found, 1 -> found, -1 -> system error
            r = service_isenabled(deps.s + id) ;
            if (r == -1)
                log_dieu(LOG_EXIT_SYS, "check already enabled services") ;

            if (r > 0) {
                if (!ext)
                    break ;
                else
                    continue ;
            }

            r = service_frontend_path(&sa, deps.s + id, info->owner, directory_forced) ;
            if (r == -1)
                goto err ;

            if (!r) {

                log_trace("unable to find", ext ? " external " : " optional ", "dependency: ", deps.s + id, " for service: ", keep.s + alltype->cname.name) ;

                // external deps must exist
                if (ext)
                    goto err ;

                continue ;
            }

            if (!parse_service(sa.s, parsed_list, info, force, alltype->overwrite_conf))
                goto err ;

            if (!sastr_add_string(rebuild, deps.s + id))
                log_die_nomem("stralloc") ;

            // we only keep the first optsdepends found
            if (!ext) break ;
        }

    } else log_trace(keep.s + alltype->cname.name,": haven't", ext ? " external " : " optional ", "dependencies") ;

    e = 1 ;

    err:
        stralloc_free(&sa) ;
        return e ;
}

int parse_service_alldeps(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, char const *directory_forced)
{
    log_flow() ;

    int e = 0 ;
    size_t id, nid, pos ;
    char *name = keep.s + alltype->cname.name ;
    stralloc sa = STRALLOC_ZERO ;
    stralloc rebuild = STRALLOC_ZERO ;

    if (!parse_service_deps(alltype, info, parsed_list, force, directory_forced)) {
        log_warnu("parse dependencies of: ", name) ;
        goto err ;
    }

    if (!parse_service_optsdeps(&rebuild, alltype, info, parsed_list, force, KEY_MAIN_EXTDEPS,  directory_forced)) {
        log_warnu("parse external dependencies of: ", name) ;
        goto err ;
    }

    if (!parse_service_optsdeps(&rebuild, alltype, info, parsed_list, force, KEY_MAIN_OPTSDEPS,  directory_forced)) {
        log_warnu("parse optional dependencies of: ", name) ;
        goto err ;
    }

    // rebuild the dependencies list of the service to add the new dependencies.
    if (rebuild.len) {

        pos = 0 ;
        sa.len = 0 ;
        id = alltype->cname.idga ;
        nid = alltype->cname.nga ;

        {
            for (; nid ; id += strlen(deps.s + id) + 1, nid--) {
                if (!sastr_add_string(&sa, deps.s + id)) {
                    log_warn("stralloc") ;
                    goto err ;
                }
            }


            FOREACH_SASTR(&rebuild, pos) {
                if (!sastr_add_string(&sa, rebuild.s + pos)) {
                    log_warn("stralloc") ;
                    goto err ;
                }
            }

        }

        alltype->cname.idga = deps.len ;
        alltype->cname.nga = 0 ;

        pos = 0 ;
        FOREACH_SASTR(&sa, pos) {

            log_info("rebuil list", sa.s + pos) ;

            if (!sastr_add_string(&deps,sa.s + pos)) {
                log_warn("stralloc") ;
                goto err ;
            }

            alltype->cname.nga++ ;
        }
    }

    e = 1 ;

    err:
        stralloc_free(&rebuild) ;
        stralloc_free(&sa) ;

    return e ;
}
