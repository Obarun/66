/*
 * ssexec_remove.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>
#include <oblibs/hash2.h>
#include <oblibs/files.h>

#include <66/state.h>
#include <66/enum_parser.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/symlink.h>

static void auto_remove(char const *path)
{
    log_trace("remove directory: ", path) ;
    if (!dir_destroy(path))
        log_dieusys(LOG_EXIT_SYS, "remove directory: ", path) ;
}

static void compute_deps(resolve_service_t *res, hash_t *hres, strbuf *sa, ssexec_t *info, uint8_t propagate)
{
    log_flow() ;

    if (!res->dependencies.nrequiredby)
        return ;

    int r ;
    unsigned int pos = 0 ;
    ss_state_t ste = STATE_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;
    _alloc_sbl_(stk, strlen(res->sa.s + res->dependencies.requiredby) + 1) ;

    if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.requiredby))
        log_dieu(LOG_EXIT_SYS, "convert string") ;

    if (propagate)
        log_1_warn("service: ", res->sa.s + res->name," is needed by its required-by dependencies: ", res->sa.s + res->dependencies.requiredby) ;

    FOREACH_SBL(&stk, pos) {

        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &dres) ;

        r = resolve_read_g(wres, info->base.s, stk.s + pos) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", stk.s + pos) ;

        if (!r) {
            if (!propagate)
                log_warn("service: ", stk.s + pos, " doesn't exist -- ignoring it") ;
            resolve_free(wres) ;
            continue ;
        }

        if (!res->islog) {

            if (!state_read(&ste, &dres))
                log_dieusys(LOG_EXIT_SYS, "read state file of: ", stk.s + pos, " -- please make a bug report") ;

            if (ste.issupervised == STATE_FLAGS_TRUE && !propagate)
                if (!sbl_add(sa, stk.s + pos))
                    log_dieusys(LOG_EXIT_SYS, "add service: ", stk.s + pos, " to stop selection") ;

            if (!propagate) {
                log_trace("add service: ", stk.s + pos, " to the service selection") ;
                if (!resolve_hash_add(hres, stk.s + pos, dres))
                    log_dieu(LOG_EXIT_SYS, "append service selection with: ", stk.s + pos) ;
            }

            if (dres.dependencies.nrequiredby && !propagate)
                compute_deps(&dres, hres, sa, info, propagate) ;
        }

        free(wres) ;
    }
}

static void remove_provide(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    size_t pos = 0 ;

    _alloc_strbuf_(path, SS_MAX_PATH_LEN) ;
    _alloc_sbl_(stk, strlen(res->sa.s + res->dependencies.provide)) ;
    _alloc_strbuf_(lnk, info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME) ;
    _alloc_strbuf_(lname, SS_MAX_PATH_LEN) ;

    if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.provide))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SBL(&stk, pos) {

        char *name = stk.s + pos ;

        auto_strings(lnk.s, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        auto_strings(path.s, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        if (symlink_type(lnk.s) > 0) {

            auto_strings(lname.s, name) ;

            if (!service_resolve_symlink(info->base.s, path.s, lname.s)) {
                log_warnusys("resolve symlink path: ", lnk.s) ;
                continue ;
            }

            if (!strcmp(lname.s, res->sa.s + res->name)) {
                log_trace("remove provide symlink: ", lnk.s) ;
                file_tryunlink(lnk.s) ;
            }
        }
    }
}

static void clean_depends(resolve_service_t *res, ssexec_t *info, uint8_t propagate)
{
    log_flow() ;

    if (!res->dependencies.ndepends)
        return ;

    if (propagate)
        return ;

    int r ;
    size_t pos = 0 ;
    resolve_wrapper_t_ref wres = 0 ;
    _alloc_sbl_(stk, strlen(res->sa.s + res->dependencies.depends)) ;

    if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.depends))
        log_dieusys(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SBL(&stk, pos) {

        char *name = stk.s + pos ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &dres) ;

        r = resolve_read_g(wres, info->base.s, name) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", name) ;

        if (!r || dres.islog) {
            resolve_free(wres) ;
            continue ;
        }

        if (dres.dependencies.nrequiredby) {

            resolve_enum_table_t table = E_TABLE_SERVICE_DEPS_ZERO ;
            _alloc_sbl_(deps, strlen(dres.sa.s + dres.dependencies.requiredby)) ;

            if (!sbl_clean_string(&deps, dres.sa.s + dres.dependencies.requiredby))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            if (!sbl_remove(&deps, res->sa.s + res->name))
                log_dieu(LOG_EXIT_SYS, "remove service: ", res->sa.s + res->name, " from requiredby dependencies list of: ", name) ;


            if (!deps.len) {

                dres.dependencies.nrequiredby = 0 ;
                dres.dependencies.requiredby = 0 ;

            } else {

                if (!sbl_rebuild_with_delim(&deps, ' '))
                    log_dieu(LOG_EXIT_SYS, "convert stack to string") ;

                table.u.service.id = E_RESOLVE_SERVICE_DEPS_REQUIREDBY ;

                if (!resolve_modify_field(wres, table, deps.len ? deps.s : ""))
                    log_dieusys(LOG_EXIT_SYS, "modify resolve file of service: ", dres.sa.s + dres.name) ;
            }

            if (!resolve_write_g(wres, info->base.s, dres.sa.s + dres.name))
                log_dieusys(LOG_EXIT_SYS, "write resolve file of service: ", dres.sa.s + dres.name) ;

        }
        resolve_free(wres) ;
    }
}

static void remove_logger(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->logger.name ;
    resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref lwres = resolve_set_struct(DATA_SERVICE, &lres) ;

    if (res->type == E_PARSER_TYPE_ONESHOT) {

        auto_remove(res->sa.s + res->io.fdout.destination) ;
        log_info("Removed successfully logger of: ", res->sa.s + res->name) ;
        resolve_free(lwres) ;
        return ;

    }

    r = resolve_read_g(lwres, info->base.s, name) ;
    if (r <= 0) {
        log_warn("service: ", name, " is already removed -- ignoring it") ;
        resolve_free(lwres) ;
        return ;
    }

    char sym[strlen(lres.sa.s + lres.path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + strlen(lres.sa.s + lres.name) + 1] ;

    auto_strings(sym, lres.sa.s + lres.path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", lres.sa.s + lres.name) ;

    auto_remove(lres.sa.s + lres.path.servicedir) ;

    auto_remove(lres.sa.s + lres.io.fdout.destination) ;

    tree_service_remove(info->base.s, lres.sa.s + lres.treename, lres.sa.s + lres.name) ;

    log_trace("remove symlink: ", sym) ;
    file_tryunlink(sym) ;

    log_trace("remove symlink: ", lres.sa.s + lres.live.scandir) ;
    file_tryunlink(lres.sa.s + lres.live.scandir) ;

    log_info("Removed successfully: ", lres.sa.s + lres.name) ;

    resolve_free(lwres) ;
}

static void remove_service(resolve_service_t *res, ssexec_t *info, uint8_t propagate)
{
    log_flow() ;

    if (res->islog)
        return ;

    if (res->dependencies.nprovide)
        remove_provide(res, info) ;

    if (res->logger.want)
        remove_logger(res, info) ;

    if (res->dependencies.ndepends)
        clean_depends(res, info, propagate) ;

    char sym[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    auto_remove(res->sa.s + res->path.servicedir) ;

    if (res->environ.envdir)
        auto_remove(res->sa.s + res->environ.envdir) ;

    tree_service_remove(info->base.s, res->sa.s + res->treename, res->sa.s + res->name) ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", res->sa.s + res->name) ;

    log_trace("remove symlink: ", sym) ;
    file_tryunlink(sym) ;

    log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
    file_tryunlink(res->sa.s + res->live.scandir) ;

    log_info("Removed successfully: ", res->sa.s + res->name) ;
}

static opt_t const opts_remove[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies at stop process" },
    { .id = 'f',         .shortname = 'f',                             .arg = OPT_NONE, .help = "force removal of a service that is part of a module", .hidden = true },
} ;

static uint8_t opt_nopropagate = 0 ;
static uint8_t opt_force = 0 ; // force is an inner option used by parse_module to delete service inside module

static int on_remove(int id, char const *arg, void *data)
{
    (void)arg ; (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;

        case 'f' :

            opt_force = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_remove = {
    .name = "66 remove",
    .help = "remove services and cleanup all files belong to it from the system",
    .operands = "service...",
    .opts = opts_remove,
    .nopts = OPT_COUNT(opts_remove),
    .on_option = &on_remove,
    .fn = &ssexec_remove,
} ;

int ssexec_remove(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    int r ;
    size_t pos = 0 ;
    uint8_t propagate = opt_nopropagate ;
    uint8_t force = opt_force ;
    opt_nopropagate = 0 ;
    opt_force = 0 ;
    ss_state_t ste = STATE_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;
    hash_t hres = HASH_ZERO ;
    struct resolve_hash_s *c, *tmp ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if (!hash_init(&hres, 0, offsetof(struct resolve_hash_s, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize hash table") ;

    for(; pos < (size_t)argc ; pos++) {

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &res) ;

        r = resolve_read_g(wres, info->base.s, argv[pos]) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", argv[pos]) ;

        if (!r)
            log_dieu(LOG_EXIT_USER, "find service: ", argv[pos], " -- did you parse it?") ;

        if (res.inns && !force)
            log_die(LOG_EXIT_USER, "service: ", argv[pos]," is part of a module and cannot be removed alone -- please remove the entire module instead using \'66 remove ", res.sa.s + res.inns, "\'") ;

        if (!res.islog) {

            if (!state_read(&ste, &res)) {
                /** Considere it down. We are on remove command, it should success
                 * whatever its state*/
                log_warnusys("read state file of: ", argv[pos], " -- ignoring its state") ;
                goto add ;
            }

            if (ste.issupervised == STATE_FLAGS_TRUE) {
                /** services of group boot cannot be stopped, the changes will appear only at
                 * next reboot.*/
                r = tree_ongroups(res.sa.s + res.path.home, res.sa.s + res.treename, TREE_GROUPS_BOOT) ;

                if (r < 0)
                    log_dieu(LOG_EXIT_SYS, "get groups of service: ", argv[pos]) ;

                if (!r)
                    if (!sbl_add(&sa, argv[pos]))
                        log_dieusys(LOG_EXIT_SYS, "add service: ", argv[pos], " to stop selection") ;
            }
        add:
            log_trace("add service: ", argv[pos], " to the service selection") ;
            if (!resolve_hash_add(&hres, argv[pos], res))
                log_dieu(LOG_EXIT_SYS, "append service selection with: ", argv[pos]) ;

            compute_deps(&res, &hres, &sa, info, propagate) ;
        }
    }

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;

    if (sa.len && r) {

        pos = 0 ;
        char const *prog = PROG ;
        int nargc = 2 + propagate + sbl_count(&sa) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        if (propagate)
            newargv[m++] = "-P" ;

        FOREACH_SBL(&sa, pos)
            newargv[m++] = sa.s + pos ;

        newargv[m] = 0 ;

        PROG = "stop" ;
        /** TODO, it should be a new process
         * to avoid to crash. This is the remove process,
         * and should always return true as the main goal is
         * to remove the service. */
        if (opt_dispatch(nargc, newargv, &cmd_stop, info))
            log_dieu(LOG_EXIT_SYS, "stop service selection") ;
        PROG = prog ;
    }

    HASH_FOREACH(&hres, c, tmp) {

        remove_service(&c->res, info, propagate) ;

        if (c->res.dependencies.ncontents && c->res.type == E_PARSER_TYPE_MODULE) {

            size_t pos = 0 ;
            resolve_service_t mres = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &mres) ;
            _alloc_sbl_(stk, strlen(c->res.sa.s + c->res.dependencies.contents) + 1) ;

            if (!sbl_clean_string(&stk, c->res.sa.s + c->res.dependencies.contents))
                log_dieu(LOG_EXIT_SYS, "convert string") ;

            FOREACH_SBL(&stk, pos) {

                r = resolve_read_g(dwres, info->base.s, stk.s + pos) ;
                if (r <= 0) {
                    log_warnusys("read resolve file of: ", stk.s + pos) ;
                    continue ;
                }

                remove_service(&mres, info, propagate) ;
            }
            resolve_free(dwres) ;
        }
    }

    resolve_hash_free(&hres) ;
    free(wres) ;

    return 0 ;
}
