/*
 * parse_compute_resolve.c
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

static uint32_t compute_src_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->base.len + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

    auto_strings(dir, info->base.s, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;

    return resolve_add_string(wres, dir) ;
}

static uint32_t compute_live_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;

    auto_strings(dir, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, dir) ;
}


static uint32_t compute_status(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(dir, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name, SS_STATE, "/", SS_STATUS) ;

    return resolve_add_string(wres, dir) ;

}

static uint32_t compute_scan_dir(resolve_wrapper_t_ref wres, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = (resolve_service_t *)wres->obj ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;

    char dir[info->live.len + SS_SCANDIR_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;

    auto_strings(dir, info->live.s, SS_SCANDIR, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, dir) ;
}

static uint32_t compute_state_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *folder)
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

static uint32_t compute_pipe_service(resolve_wrapper_t_ref wres, ssexec_t *info, char const *service, char const *name)
{
    log_flow() ;

    size_t namelen = strlen(name) ;

    char tmp[info->live.len + SS_SCANDIR_LEN + 1 + info->ownerlen + 1 + namelen + 1] ;
    auto_strings(tmp, info->live.s, SS_SCANDIR, "/", info->ownerstr, "/", name) ;

    return resolve_add_string(wres, tmp) ;

}

static uint32_t compute_log_dir(resolve_wrapper_t_ref wres, resolve_service_t *res)
{
    log_flow() ;

    size_t namelen = strlen(res->sa.s + res->name) ;
    size_t syslen = res->owner ? strlen(res->sa.s + res->path.home) + strlen(SS_LOGGER_USERDIR) : strlen(SS_LOGGER_SYSDIR) ;
    size_t dstlen = res->logger.destination ? strlen(res->sa.s + res->logger.destination) : strlen(SS_LOGGER_SYSDIR) ;

    char dstlog[syslen + dstlen + namelen + 1] ;

    if (!res->logger.destination) {

        if (res->owner) {

            char home[SS_MAX_PATH_LEN + 1 + strlen(SS_LOGGER_USERDIR) + 1] ;

            if (!set_ownerhome_stack(home))
                log_dieusys(LOG_EXIT_SYS,"set home directory") ;

            auto_strings(dstlog, home, SS_LOGGER_USERDIR, res->sa.s + res->name) ;

        } else

            auto_strings(dstlog, SS_LOGGER_SYSDIR, res->sa.s + res->name) ;

    } else {

        auto_strings(dstlog, res->sa.s + res->logger.destination) ;
    }

    return resolve_add_string(wres, dstlog) ;
}

/**
 * @!runorfinish -> finish, @runorfinish -> run
 * */
static void compute_wrapper_scripts(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, char const *file, uint8_t runorfinish)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -" ;
    size_t shebanglen = strlen(shebang) ;

    char run[shebanglen + strlen(res->sa.s + res->live.fdholderdir) + SS_FDHOLDER_PIPENAME_LEN + strlen(res->sa.s + res->name) + SS_LOG_SUFFIX_LEN + strlen(S6_BINPREFIX) + (SS_MAX_PATH*2) + SS_MAX_PATH + strlen(file) + 132 + 1] ;

    auto_strings(run, shebang, !runorfinish ? (res->type != TYPE_ONESHOT ? "S0\n" : "P\n") : "P\n") ;

    if (res->logger.want && res->type != TYPE_ONESHOT)
        auto_strings(run + FAKELEN, \
                "fdmove 1 0\n", \
                "s6-fdholder-retrieve ", \
                res->sa.s + res->live.fdholderdir, "/s ", \
                "\"" SS_FDHOLDER_PIPENAME "w-", \
                res->sa.s + res->name, SS_LOG_SUFFIX "\"\n", \
                "fdswap 0 1\n") ;

    auto_strings(run + FAKELEN, "./", file, ".user", !runorfinish ? (res->type != TYPE_ONESHOT ? " $@\n" : "\n") : "\n") ;

    if (runorfinish)
        res->execute.run.run = resolve_add_string(wres, run) ;
    else
        res->execute.finish.run = resolve_add_string(wres, run) ;

    free(wres) ;
}

/**
 * @!runorfinish -> finish, @runofinish -> run
 * */
static void compute_wrapper_scripts_user(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, uint8_t runorfinish)
{

    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = scripts->shebang ? res->sa.s + scripts->shebang : SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
    size_t shebanglen = strlen(shebang) ;
    size_t scriptlen = strlen(res->sa.s + scripts->run_user) ;
    size_t loglen = (res->logger.want && res->type == TYPE_ONESHOT) ? strlen(res->sa.s + res->logger.destination) : 0 ;
    size_t runaslen = (!res->owner && scripts->runas) ? strlen(res->sa.s + scripts->runas) : 0 ;
    int build = !strcmp(res->sa.s + scripts->build, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;
    size_t execlen = !build ? (res->environ.envdir ? (strlen(res->sa.s + res->environ.envdir) + 19 + SS_SYM_VERSION_LEN + 1) : 0) : 0 ;

    char run[shebanglen + (loglen + 22) + 14 + execlen + (runaslen + 14) + scriptlen + 4 + 1] ;

    auto_strings(run, "#!") ;

    if (scripts->shebang)
        log_warn("@shebang field is deprecated -- please define it at start of your @execute field instead") ;

    if (build && scripts->shebang) {

        auto_strings(run + FAKELEN, res->sa.s + scripts->shebang, "\n") ;

    } else {

        auto_strings(run + FAKELEN, shebang) ;

        if (res->logger.want && res->type == TYPE_ONESHOT)
            auto_strings(run + FAKELEN, "redirfd -a 1 ", res->sa.s + res->logger.destination, "/current\n") ;

        auto_strings(run + FAKELEN, "fdmove -c 2 1\n") ;

        if (res->environ.envdir)
            auto_strings(run + FAKELEN, "execl-envfile -v4 ", res->sa.s + res->environ.envdir, SS_SYM_VERSION "\n") ;

        /** runas */
        if (!res->owner && scripts->runas)
            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", res->sa.s + scripts->runas, "\n") ;

    }

    auto_strings(run + FAKELEN, res->sa.s + scripts->run_user, "\n") ;

    if (runorfinish)
        res->execute.run.run_user = resolve_add_string(wres, run) ;
    else
        res->execute.finish.run_user = resolve_add_string(wres, run) ;

    free(wres) ;
}

static void compute_log_script(resolve_service_t *res)
{

    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    int build = !strcmp(res->sa.s + res->logger.execute.run.build, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;

    char *pmax = 0 ;
    char *pback = 0 ;
    char max[UINT32_FMT] ;
    char back[UINT32_FMT] ;
    char *timestamp = 0 ;
    int itimestamp = SS_LOGGER_TIMESTAMP ;
    char *logrunner = res->logger.execute.run.runas ? res->sa.s + res->logger.execute.run.runas : SS_LOGGER_RUNNER ;

    res->logger.execute.run.runas = resolve_add_string(wres, logrunner) ;

    /** timestamp */
    if (res->logger.timestamp != 3)
        timestamp = res->logger.timestamp == TIME_NONE ? "" : res->logger.timestamp == TIME_ISO ? "T" : "t" ;
    else
        timestamp = itimestamp == TIME_NONE ? "" : itimestamp == TIME_ISO ? "T" : "t" ;

    /** backup */
    if (res->logger.backup) {

        back[uint32_fmt(back,res->logger.backup)] = 0 ;
        pback = back ;

    }

    /** file size */
    if (res->logger.maxsize) {

        max[uint32_fmt(max,res->logger.maxsize)] = 0 ;
        pmax = max ;

    }

    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
    size_t shebanglen = strlen(shebang) ;

    {
        /** run scripts */
        char run[strlen(shebang) + strlen(res->sa.s + res->live.fdholderdir) + SS_FDHOLDER_PIPENAME_LEN +  strlen(res->sa.s + res->logger.name) + strlen(logrunner) + strlen(S6_BINPREFIX) + strlen(res->sa.s + res->logger.execute.run.runas) + 67 + 1] ;

        auto_strings(run, \
                    shebang, \
                    "s6-fdholder-retrieve ", \
                    res->sa.s + res->live.fdholderdir, "/s ", \
                    "\"" SS_FDHOLDER_PIPENAME "r-", \
                    res->sa.s + res->logger.name, "\"\n") ;

        auto_strings(run + FAKELEN, "./run.user\n") ;

        res->logger.execute.run.run = resolve_add_string(wres, run) ;

    }

    {
        if (!build) {
            /** run.user script */
            char run[shebanglen + strlen(pback) + strlen(timestamp) + strlen(pmax) + strlen(res->sa.s + res->logger.destination) + 17 + 1] ;

            auto_strings(run, shebang) ;

            auto_strings(run + FAKELEN, "fdmove -c 2 1\n") ;

            /** runas */
            if (!res->owner)
                auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", logrunner, "\n") ;


            auto_strings(run + FAKELEN, "s6-log ") ;

            if (SS_LOGGER_NOTIFY)
                auto_strings(run + FAKELEN, "-d3 ") ;

            auto_strings(run + FAKELEN, "n", pback, " ") ;

            if (res->logger.timestamp < TIME_NONE)
                auto_strings(run + FAKELEN, timestamp, " ") ;

            auto_strings(run + FAKELEN, "s", pmax, " ", res->sa.s + res->logger.destination, "\n") ;

            res->logger.execute.run.run_user = resolve_add_string(wres, run) ;

        } else {

            if (res->logger.execute.run.shebang)
                log_warn("@shebang field is deprecated -- please define it at start of your @execute field instead") ;

            char *shebang = res->logger.execute.run.shebang ? res->sa.s + res->logger.execute.run.shebang : "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
            size_t shebanglen = strlen(shebang) ;

            char run[shebanglen + strlen(res->sa.s + res->logger.execute.run.run_user) + 2] ;
            auto_strings(run, shebang, res->sa.s + res->logger.execute.run.run_user, "\n") ;

            res->logger.execute.run.run_user = resolve_add_string(wres, run) ;
        }
    }

    free(wres) ;
}

static void compute_log(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info)
{
    log_flow() ;

    if (!res->logger.name)
        return ;

    resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &lres) ;

    resolve_init(wres) ;

    char *str = res->sa.s ;
    size_t namelen = strlen(str + res->logger.name) ;
    char name[namelen + 1] ;
    char description[namelen + 7 + 1] ;

    auto_strings(name, str + res->logger.name) ;

    auto_strings(description, str + res->name, " logger") ;

    lres.name = resolve_add_string(wres, name) ;
    lres.description = resolve_add_string(wres, description) ;
    lres.version = resolve_add_string(wres, str + res->version) ;
    lres.type = res->type ;
    lres.notify = 3 ;
    lres.maxdeath = res->maxdeath ;
    lres.earlier = res->earlier ;
    if (res->intree)
        lres.intree = resolve_add_string(wres, str + res->intree) ;

    lres.ownerstr = resolve_add_string(wres, str + res->ownerstr) ;
    lres.owner = res->owner ;
    lres.treename = resolve_add_string(wres, str + res->treename) ;
    lres.user = resolve_add_string(wres, str + res->user) ;
    if (res->inns)
        lres.inns = resolve_add_string(wres, str + res->inns) ;

    lres.path.home = resolve_add_string(wres, str + res->path.home) ;
    lres.path.frontend = resolve_add_string(wres, str + res->path.frontend) ;
    lres.path.servicedir = compute_src_servicedir(wres, info) ;

    lres.dependencies.requiredby = resolve_add_string(wres, str + res->name) ;
    lres.dependencies.nrequiredby = 1 ;

    lres.execute.run.build = resolve_add_string(wres, str + res->logger.execute.run.build) ;
    lres.execute.run.shebang = res->logger.execute.run.shebang ? resolve_add_string(wres, str + res->logger.execute.run.shebang) : 0 ;
    lres.execute.run.runas = res->logger.execute.run.runas ? resolve_add_string(wres, str + res->logger.execute.run.runas) : resolve_add_string(wres, SS_LOGGER_RUNNER) ;
    lres.execute.timeout.kill = res->logger.execute.timeout.kill ;
    lres.execute.timeout.finish = res->logger.execute.timeout.finish ;
    lres.execute.down = res->logger.execute.down ;
    lres.execute.downsignal = res->logger.execute.downsignal ;

    lres.live.livedir = resolve_add_string(wres, info->live.s) ;
    lres.live.status = compute_status(wres, info) ;
    lres.live.servicedir = compute_live_servicedir(wres, info) ;
    lres.live.scandir = compute_scan_dir(wres, info) ;
    lres.live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;
    lres.live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;
    lres.live.notifdir = compute_state_dir(wres, info, "notif") ;
    lres.live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;
    lres.live.fdholderdir = compute_pipe_service(wres, info, name, SS_FDHOLDER) ;
    lres.live.oneshotddir = compute_pipe_service(wres, info, name, SS_ONESHOTD) ;

    lres.logger.destination = resolve_add_string(wres, str + res->logger.destination) ;
    lres.logger.backup = res->logger.backup ;
    lres.logger.maxsize = res->logger.maxsize ;
    lres.logger.timestamp = res->logger.timestamp ;
    lres.logger.want = 0 ;

    // oneshot do not use fdholder daemon
    if (res->type == TYPE_CLASSIC) {

        compute_log_script(res) ;
        lres.execute.run.run = resolve_add_string(wres, res->sa.s + res->logger.execute.run.run) ;
        lres.execute.run.run_user = resolve_add_string(wres, res->sa.s + res->logger.execute.run.run_user) ;
        lres.execute.run.runas = resolve_add_string(wres, res->sa.s + res->logger.execute.run.runas) ;
    }

    if (service_resolve_array_search(ares, *areslen, name) < 0) {

        log_trace("add service ", name, " to the selection") ;
        if (*areslen > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

        ares[(*areslen)++] = lres ;
    }

    free(wres) ;
}

void parse_compute_resolve(unsigned int idx, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t_ref res = &ares[idx] ;
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
    res->live.fdholderdir = compute_pipe_service(wres, info, name, SS_FDHOLDER) ;

    /* oneshotd */
    res->live.oneshotddir = compute_pipe_service(wres, info, name, SS_ONESHOTD) ;

    if (res->logger.want && (res->type == TYPE_CLASSIC || res->type == TYPE_ONESHOT)) {

        size_t namelen = strlen(name) ;

        char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

        auto_strings(logname, name, SS_LOG_SUFFIX) ;

        res->logger.name = resolve_add_string(wres, logname) ;

        res->logger.destination = compute_log_dir(wres, res) ;

        if (res->type == TYPE_CLASSIC) {

            /** the logger is not a service with oneshot type */
            if (res->dependencies.ndepends) {

                char buf[strlen(res->sa.s + res->dependencies.depends) + 1 + strlen(res->sa.s + res->logger.name) + 1] ;
                auto_strings(buf, res->sa.s + res->dependencies.depends, " ", res->sa.s + res->logger.name) ;

                res->dependencies.depends = resolve_add_string(wres, buf) ;

            } else {

                res->dependencies.depends = resolve_add_string(wres, res->sa.s + res->logger.name) ;
            }

            res->dependencies.ndepends++ ;

            compute_log(res, ares, areslen, info) ;
        }
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
