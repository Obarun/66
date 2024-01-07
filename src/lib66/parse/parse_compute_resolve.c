/*
 * parse_compute_resolve.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#include <66/enum.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/ssexec.h>
#include <66/parse.h>
#include <66/utils.h>
#include <66/service.h>
#include <66/sanitize.h>
#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

uint32_t compute_src_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->base.len + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

    auto_strings(dir, info->base.s, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;

    return resolve_add_string(wres, dir) ;
}

uint32_t compute_live_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;

    auto_strings(dir, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, dir) ;
}


uint32_t compute_status(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(dir, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name, SS_STATE, "/", SS_STATUS) ;

    return resolve_add_string(wres, dir) ;

}

uint32_t compute_scan_dir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->live.len + SS_SCANDIR_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;

    auto_strings(dir, info->live.s, SS_SCANDIR, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, dir) ;
}

uint32_t compute_state_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *folder)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t folderlen = strlen(folder) ;

    char dir[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + namelen + 1 + folderlen + 1] ;

    auto_strings(dir, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", name, "/", folder) ;

    return resolve_add_string(wres, dir) ;
}

uint32_t compute_pipe_service(resolve_wrapper_t_ref wres, ssexec_t *info, char const *name)
{
    log_flow() ;

    size_t namelen = strlen(name) ;

    char tmp[info->live.len + SS_SCANDIR_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;
    auto_strings(tmp, info->live.s, SS_SCANDIR, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, tmp) ;

}

/**
 * @!runorfinish -> finish, @runorfinish -> run
 * */
static void compute_wrapper_scripts(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, char const *file, uint8_t runorfinish)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -" ;

    char run[SS_MAX_PATH_LEN + 1] ;

    auto_strings(run, shebang, !runorfinish ? (res->type != TYPE_ONESHOT ? "S0\n" : "P\n") : "P\n") ;

    if (res->logger.want && res->type != TYPE_ONESHOT)
        auto_strings(run + FAKELEN, \
                SS_EXECLINE_SHEBANGPREFIX "fdmove 1 0\n", \
                S6_BINPREFIX "s6-fdholder-retrieve ", \
                res->sa.s + res->live.fdholderdir, "/s ", \
                "\"" SS_FDHOLDER_PIPENAME "w-", \
                res->sa.s + res->name, SS_LOG_SUFFIX "\"\n", \
                SS_EXECLINE_SHEBANGPREFIX "fdswap 0 1\n") ;

    auto_strings(run + FAKELEN, "./", file, ".user", !runorfinish ? (res->type != TYPE_ONESHOT ? " $@\n" : "\n") : "\n") ;

    if (runorfinish)
        res->execute.run.run = resolve_add_string(wres, run) ;
    else
        res->execute.finish.run = resolve_add_string(wres, run) ;

    free(wres) ;
}

/**
 * @!runorfinish -> finish.user, @runofinish -> run.user
 * */
static void compute_wrapper_scripts_user(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, uint8_t runorfinish)
{

    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = 0 ;
    size_t scriptlen = strlen(res->sa.s + scripts->run_user) ;
    size_t loglen = (res->logger.want && res->type == TYPE_ONESHOT) ? strlen(res->sa.s + res->logger.destination) : 0 ;
    size_t runaslen = (!res->owner && scripts->runas) ? strlen(res->sa.s + scripts->runas) : 0 ;
    int build = !strcmp(res->sa.s + scripts->build, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;
    size_t execlen = !build ? (res->environ.envdir ? (strlen(res->sa.s + res->environ.envdir) + 19 + SS_SYM_VERSION_LEN + 1) : 0) : 0 ;
    size_t fakelen = 0, shebanglen = 0 ;
    /** shebang is deprecated*/
    if (scripts->shebang)
        log_warn("@shebang field is deprecated -- please define it at the start of your @execute field instead") ;

    if (build && scripts->shebang) {
        shebang = res->sa.s + scripts->shebang ;
        shebanglen = strlen(shebang) ;
    } else if (!build) {
        shebang = SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
        shebanglen = strlen(shebang) ;
    }

    char run[shebanglen + (loglen + 22) + 14 + execlen + (runaslen + 14) + scriptlen + 4 + 1] ;


    if (build && scripts->shebang) {

        auto_strings(run, "#!") ;
        auto_strings(run + FAKELEN, res->sa.s + scripts->shebang, "\n") ;
        fakelen = FAKELEN ;

    } else if (!build) {

        auto_strings(run, "#!") ;
        auto_strings(run + FAKELEN, shebang) ;

        if (res->logger.want && res->type == TYPE_ONESHOT)
            auto_strings(run + FAKELEN, SS_EXECLINE_SHEBANGPREFIX "redirfd -a 1 ", res->sa.s + res->logger.destination, "/current\n") ;

        auto_strings(run + FAKELEN, SS_EXECLINE_SHEBANGPREFIX "fdmove -c 2 1\n") ;

        if (res->environ.envdir)
            auto_strings(run + FAKELEN, "execl-envfile -v4 ", res->sa.s + res->environ.envdir, SS_SYM_VERSION "\n") ;

        /** runas */
        if (!res->owner && scripts->runas)
            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", res->sa.s + scripts->runas, "\n") ;

        fakelen = FAKELEN ;

    }

    auto_strings(run + fakelen, res->sa.s + scripts->run_user, "\n") ;

    if (runorfinish)
        res->execute.run.run_user = resolve_add_string(wres, run) ;
    else
        res->execute.finish.run_user = resolve_add_string(wres, run) ;

    free(wres) ;
}

void parse_compute_resolve(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char name[strlen(res->sa.s + res->name) + 1] ;

    auto_strings(name, res->sa.s + res->name) ;

    res->path.servicedir = compute_src_servicedir(wres, info) ;

    /* live */
    res->live.livedir = resolve_add_string(wres, info->live.s) ;

    /* status */
    res->live.status = compute_status(wres, info) ;

    /* servicedir */
    res->live.servicedir = compute_live_servicedir(wres, info) ;

    /* scandir */
    res->live.scandir = compute_scan_dir(wres, info) ;

    /* state */
    res->live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;

    /* event */
    res->live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;

    /* notif */
    res->live.notifdir = compute_state_dir(wres, info, "notif") ;

    /* supervise */
    res->live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;

    /* fdholder */
    res->live.fdholderdir = compute_pipe_service(wres, info, SS_FDHOLDER) ;

    /* oneshotd */
    res->live.oneshotddir = compute_pipe_service(wres, info, SS_ONESHOTD) ;

    if (res->logger.want && res->type != TYPE_MODULE && !res->inns) {

        char *name = res->sa.s + res->name ;
        size_t namelen = strlen(name) ;
        char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

        auto_strings(logname, name, SS_LOG_SUFFIX) ;

        res->logger.name = resolve_add_string(wres, logname) ;

        res->logger.destination = compute_log_dir(wres, res) ;

        res->logger.execute.run.runas = res->logger.execute.run.runas ? resolve_add_string(wres, res->sa.s + res->logger.execute.run.runas) : resolve_add_string(wres, SS_LOGGER_RUNNER) ;
    }

    if (res->type == TYPE_ONESHOT || res->type == TYPE_CLASSIC) {

        // {run,up}/{run,up}.user script
        if (res->type == TYPE_ONESHOT) {

            compute_wrapper_scripts(res, &res->execute.run, "up", 1) ;
            compute_wrapper_scripts_user(res, &res->execute.run, 1) ;

        } else {

            compute_wrapper_scripts(res, &res->execute.run, "run", 1) ;
            compute_wrapper_scripts_user(res, &res->execute.run, 1) ;

        }

        // {finish,down}/{finish,down}.user script
        if (res->execute.finish.run_user) {

            if (res->type == TYPE_ONESHOT) {

                compute_wrapper_scripts(res, &res->execute.finish, "down", 0) ;
                compute_wrapper_scripts_user(res, &res->execute.finish, 0) ;

            } else {

                compute_wrapper_scripts(res, &res->execute.finish, "finish", 0) ;
                compute_wrapper_scripts_user(res, &res->execute.finish, 0) ;

            }
        }
    }

    free(wres) ;
}
