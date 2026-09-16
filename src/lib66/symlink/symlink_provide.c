/*
 * symlink_provide.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/symlink.h>
#include <66/constants.h>
#include <66/enum_service.h>
#include <66/module.h>

static int provide_holder(char *holder, char const *base, char const *name)
{
    log_flow() ;

    if (!service_resolve_provide(holder, name, base))
        log_warnusys_return(LOG_EXIT_ZERO, "resolve provide alias: ", name) ;

    if (!strcmp(holder, name))
        holder[0] = 0 ;

    return 1 ;
}

static int provide_list(strbuf *stk, char const *base, char const *service)
{
    log_flow() ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;

    int r = resolve_read(wdep, base, service) ;
    if (r < 0) {
        resolve_free(wdep) ;
        log_warnusys_return(LOG_EXIT_ZERO, "read dependencies addon of: ", service) ;
    }

    if (r && dep.nprovide && !sbl_clean_string(stk, dep.sa.s + dep.provide)) {
        resolve_free(wdep) ;
        log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;
    }

    resolve_free(wdep) ;
    return 1 ;
}

static int provide_replace_requiredby(char const *base, char const *holder, char const *name)
{
    log_flow() ;

    int e = 0 ;
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;

    // it depends on nothing and nobody
    if (!resolve_check(wdep, base, holder)) {
        free(wdep) ;
        return 1 ;
    }

    if (resolve_read(wdep, base, holder) <= 0) {
        resolve_free(wdep) ;
        log_warnusys_return(LOG_EXIT_ZERO, "read dependencies addon of: ", holder) ;
    }

    if (!dep.nrequiredby) {
        resolve_free(wdep) ;
        return 1 ;
    }

    _alloc_sbl_(stk, strlen(dep.sa.s + dep.requiredby) + 1) ;

    if (!sbl_clean_string(&stk, dep.sa.s + dep.requiredby)) {
        log_warnusys("clean string") ;
        goto freed ;
    }

    /** backwards: removing an element only moves the ones after it */
    for (unsigned int id = sbl_count(&stk) ; id-- ; ) {

        ssize_t off = sbl_element_byid(&stk, id) ;
        if (off < 0) {
            log_warnu("get the requiredby list element of: ", holder) ;
            goto freed ;
        }

        char const *consumer = stk.s + off ;
        resolve_service_addon_dependencies_t cdep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t_ref wc = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &cdep) ;

        if (resolve_check(wc, base, consumer)) {

            if (resolve_read(wc, base, consumer) <= 0) {
                log_warnusys("read dependencies addon of: ", consumer) ;
                resolve_free(wc) ;
                goto freed ;
            }

            if (cdep.ndepends) {

                _alloc_sbl_(deps, strlen(cdep.sa.s + cdep.depends) + 1) ;

                if (!sbl_clean_string(&deps, cdep.sa.s + cdep.depends)) {
                    log_warnusys("clean string") ;
                    resolve_free(wc) ;
                    goto freed ;
                }

                if (sbl_search(&deps, name) >= 0 && sbl_search(&deps, holder) < 0) {

                    log_trace("service: ", consumer, " reaches: ", holder, " through: ", name, " -- dropping it from its required-by") ;

                    if (!sbl_remove_at(&stk, off)) {
                        log_warnusys("remove service: ", consumer, " from requiredby dependencies list of: ", holder) ;
                        resolve_free(wc) ;
                        goto freed ;
                    }
                }
            }
        }

        resolve_free(wc) ;
    }

    if (sbl_count(&stk) != dep.nrequiredby) {

        resolve_enum_table_t table = E_TABLE_SERVICE_DEPS_ZERO ;

        dep.nrequiredby = sbl_count(&stk) ;

        if (!stk.len) {

            dep.requiredby = 0 ;

        } else {

            if (!sbl_rebuild_with_delim(&stk, ' ')) {
                log_warnu("convert strbuf to string") ;
                goto freed ;
            }

            table.u.service.id = E_RESOLVE_SERVICE_DEPS_REQUIREDBY ;

            if (!resolve_modify_field_by(wdep, table, stk.s)) {
                log_warnusys("modify requiredby of: ", holder) ;
                goto freed ;
            }
        }

        if (!resolve_write(wdep, base, holder)) {
            log_warnusys("write dependencies addon of: ", holder) ;
            goto freed ;
        }
    }

    e = 1 ;

    freed:
        resolve_free(wdep) ;
        return e ;
}

static int provide_move(char const *base, char const *name, char const *from, char const *to)
{
    log_flow() ;

    size_t blen = strlen(base), nlen = strlen(name) ;
    char lnk[blen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN + 1 + nlen + 1] ;

    auto_strings(lnk, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;

    if (from[0] && !provide_replace_requiredby(base, from, name))
        return 0 ;

    file_tryunlink(lnk) ;

    if (!to[0]) {
        log_trace("name: ", name, " is answered by nobody") ;
        return 1 ;
    }

    size_t tlen = strlen(to) ;
    char target[3 + tlen + 1] ;

    auto_strings(target, "../", to) ;

    log_trace("name: ", name, " is answered by: ", to) ;
    if (symlink(target, lnk) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "make symlink: ", lnk, " to: ", target) ;

    return 1 ;
}

static int provide_own_name(char const *base, char const *service)
{
    log_flow() ;

    char holder[SS_MAX_SERVICE_NAME + 1] ;

    if (!provide_holder(holder, base, service))
        return -1 ;

    if (holder[0])
        log_die(LOG_EXIT_USER, "service: ", service, " cannot be parsed, its name is provided by: ", holder, " -- '66 remove -P ", holder, "' first") ;

    return 1 ;
}

static int provide_own_name_in_argument(hash_t *hres, char const *service)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;

    HASH_FOREACH(hres, c, tmp) {

        char const *member = c->res.sa.s + c->res.name ;

        if (!c->dependencies.nprovide || !strcmp(member, service))
            continue ;

        _alloc_sbl_(names, strlen(c->dependencies.sa.s + c->dependencies.provide) + 1) ;

        if (!sbl_clean_string(&names, c->dependencies.sa.s + c->dependencies.provide))
            log_warnusys_return(LOG_EXIT_LESSONE, "clean string") ;

        if (sbl_search(&names, service) >= 0)
            log_die(LOG_EXIT_USER, "service: ", service, " is parsed along with: ", member, ", which provides its name -- '66 remove -P <one of the two services>'") ;
    }

    return 1 ;
}

static int provide_claimed_names(char const *base, char const *service, strbuf *names)
{
    log_flow() ;

    int e = -1 ;
    size_t pos = 0 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;

    FOREACH_SBL(names, pos) {

        char const *name = names->s + pos ;

        if (resolve_check(wres, base, name))
            log_die(LOG_EXIT_USER, "service: ", service, " provides the name: ", name, ", which is the name of a parsed service -- '66 remove -P ", name, "' first") ;

        if (!provide_holder(holder, base, name))
            goto freed ;

        if (!holder[0] || !strcmp(holder, service))
            continue ;

        int r = service_isenabled(base, holder) ;
        if (r < 0) {
            log_warnusys("read resolve file of: ", holder) ;
            goto freed ;
        }

        if (r)
            log_die(LOG_EXIT_USER, "service: ", service, " provides the name: ", name, ", already provided by the enabled: ", holder, " -- '66 disable -P ", holder, "' first") ;
    }

    e = 1 ;

    freed:
        free(wres) ;
        return e ;
}

static int provide_claimed_in_argument(hash_t *hres, char const *service, strbuf *names)
{
    log_flow() ;

    size_t pos = 0 ;

    FOREACH_SBL(names, pos) {

        char const *name = names->s + pos ;

        if (!strcmp(name, service))
            continue ;

        if (resolve_hash_search(hres, name))
            log_die(LOG_EXIT_USER, "service: ", service, " provides the name: ", name, ", parsed along with it -- '66 remove -P <one of the two services>") ;
    }

    return 1 ;
}

int symlink_provide_istaken(char const *base, hash_t *hres, struct resolve_hash_s *c)
{
    log_flow() ;

    int r ;

    if (c->res.islog)
        return 1 ;

    char const *service = c->res.sa.s + c->res.name ;

    r = provide_own_name(base, service) ;
    if (r < 0)
        return r ;

    r = provide_own_name_in_argument(hres, service) ;
    if (r < 0)
        return r ;

    if (!c->dependencies.nprovide)
        return 1 ;

    _alloc_sbl_(names, strlen(c->dependencies.sa.s + c->dependencies.provide) + 1) ;

    if (!sbl_clean_string(&names, c->dependencies.sa.s + c->dependencies.provide))
        log_warnusys_return(LOG_EXIT_LESSONE, "clean string") ;

    r = provide_claimed_names(base, service, &names) ;
    if (r < 0)
        return r ;

    return provide_claimed_in_argument(hres, service, &names) ;
}

int symlink_provide_isclaimable(char const *base, resolve_service_t *res)
{
    log_flow() ;

    if (res->islog)
        return 1 ;

    char *service = res->sa.s + res->name ;
    _alloc_sbl_(stk, SS_MAX_SERVICE_NAME + 1) ;

    if (!provide_list(&stk, base, service))
        return -1 ;

    return provide_claimed_names(base, service, &stk) ;
}

int symlink_provide_isavailable(char const *base, char const *name, uid_t owner)
{
    log_flow() ;

    char holder[SS_MAX_SERVICE_NAME + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;

    if (!provide_holder(holder, base, name))
        return -1 ;

    if (!holder[0])
        return 1 ;

    int r = service_frontend_path(&sa, name, owner, 0, exclude, 2) ;
    if (r < 0)
        log_warnu_return(LOG_EXIT_LESSONE, "get frontend service file of: ", name) ;

    if (r)
        log_die(LOG_EXIT_USER, "name: ", name, " is provided by: ", holder, " -- remove it first with '66 remove -P ", holder, "' command") ;

    log_die(LOG_EXIT_USER, "name: ", name, " is provided by: ", holder, " and no service has this name -- '66 enable ", holder, "' instead") ;
}

int symlink_provide_update(const char *base, resolve_service_t *res, uint8_t event)
{
    log_flow() ;

    if (res->islog)
        return 1 ;

    char *service = res->sa.s + res->name ;
    size_t pos = 0 ;
    _alloc_sbl_(stk, SS_MAX_SERVICE_NAME + 1) ;

    if (!provide_list(&stk, base, service))
        return 0 ;

    FOREACH_SBL(&stk, pos) {

        char const *name = stk.s + pos ;
        char holder[SS_MAX_SERVICE_NAME + 1] ;

        if (!provide_holder(holder, base, name))
            return 0 ;

        switch (event) {

            /** nobody answers to the name: the parsed provider does */
            case SYMLINK_PROVIDE_PARSE :
                if (!holder[0] && !provide_move(base, name, holder, service))
                    return 0 ;
                break ;

            /** the enabled provider answers, whoever answered before */
            case SYMLINK_PROVIDE_ENABLE :
                if (strcmp(holder, service) && !provide_move(base, name, holder, service))
                    return 0 ;
                break ;

            /** the provider answering is removed: nobody answers to the name */
            case SYMLINK_PROVIDE_REMOVE :
                if (!strcmp(holder, service) && !provide_move(base, name, "", ""))
                    return 0 ;
                break ;

            default :
                return (errno = EINVAL, 0) ;
        }
    }

    return 1 ;
}
