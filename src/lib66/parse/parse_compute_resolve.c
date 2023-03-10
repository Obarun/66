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

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#include <66/enum.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/ssexec.h>
#include <66/parser.h>
#include <66/utils.h>
#include <66/service.h>

#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

static uint32_t compute_scan_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *service)
{
    log_flow() ;

    size_t servicelen = strlen(service) ;

    char tmp[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + servicelen + 1 + SS_SCANDIR_LEN + 1 + servicelen + 1] ;
    auto_strings(tmp, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", service, "/", SS_SCANDIR, "/", service) ;

    return resolve_add_string(wres, tmp) ;


}

static uint32_t compute_state_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *service, char const *folder)
{
    log_flow() ;

    size_t servicelen = strlen(service) ;
    size_t folderlen = strlen(folder) ;

    char tmp[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + servicelen + 1 + folderlen + 1] ;
    auto_strings(tmp, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", service, "/", folder) ;

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

        if (res->owner)
            // + strlen(SS_USER_DIR) to avoid double SS_USER_DIR e.g. /home/<user>/.66/.66/log
            auto_strings(dstlog, res->sa.s + res->path.home, SS_LOGGER_USERDIR + strlen(SS_USER_DIR), res->sa.s + res->name) ;

        else

            auto_strings(dstlog, SS_LOGGER_SYSDIR, res->sa.s + res->name) ;

    } else {

        auto_strings(dstlog, res->sa.s + res->logger.destination) ;
    }

    return resolve_add_string(wres, dstlog) ;
}

static uint32_t compute_pipe_service(resolve_wrapper_t_ref wres, ssexec_t *info, char const *service, char const *name)
{
    log_flow() ;

    size_t servicelen = strlen(service) ;
    size_t namelen = strlen(name) ;

    char tmp[info->live.len + SS_STATE_LEN + 1 + info->ownerlen + 1 + servicelen + 1 + SS_SCANDIR_LEN + 1 + namelen + 1] ;
    auto_strings(tmp, info->live.s, SS_STATE + 1, "/", info->ownerstr, "/", service, "/", SS_SCANDIR, "/", name) ;

    return resolve_add_string(wres, tmp) ;

}

/**
 * @!runorfinish -> finish, @runorfinish -> run
 * */
static void compute_wrapper_scripts(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, char const *file, uint8_t runorfinish)
{
    log_flow() ;

    int build = !strcmp(res->sa.s + scripts->build, "custom") ? 1 : 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -" ;
    size_t shebanglen = strlen(shebang) ;
    size_t execlen = build ? (res->environ.envdir ? (strlen(res->sa.s + res->environ.envdir) + 14 + SS_SYM_VERSION_LEN + 1) : 0) : 0 ;

    char run[shebanglen + strlen(res->sa.s + res->live.fdholderdir) + SS_FDHOLDER_PIPENAME_LEN + strlen(res->sa.s + res->name) + SS_LOG_SUFFIX_LEN + strlen(S6_BINPREFIX) + strlen(res->sa.s + scripts->runas) + execlen + (SS_MAX_PATH*2) + SS_MAX_PATH + strlen(file) + 132 + 1] ;

    auto_strings(run, shebang, !runorfinish ? (res->type != TYPE_ONESHOT ? "S0\n" : "P\n") : "P\n") ;

    if (res->logger.want && res->type != TYPE_ONESHOT)
        auto_strings(run + FAKELEN, \
                "fdmove 1 0\n", \
                "s6-fdholder-retrieve ", \
                res->sa.s + res->live.fdholderdir, "/s ", \
                "\"" SS_FDHOLDER_PIPENAME "w-", \
                res->sa.s + res->name, SS_LOG_SUFFIX "\"\n", \
                "fdswap 0 1\n") ;

    auto_strings(run + FAKELEN, "fdmove -c 2 1\n") ;


    /** environ */
    if (res->environ.env && build)
        /** call execl-envfile if a custom @execute is asked.
         * its call will be made at run.user file for execlineb script*/
        auto_strings(run + FAKELEN, "execl-envfile ", res->sa.s + res->environ.envdir, SS_SYM_VERSION "\n") ;

    /** log redirection for oneshot service */
    if (res->logger.want && res->type == TYPE_ONESHOT) {

        auto_strings(run + FAKELEN, "execl-toc", " -d ", res->sa.s + res->logger.destination, " -m 0755\n") ;
        auto_strings(run + FAKELEN, "redirfd -a 1 ", res->sa.s + res->logger.destination, "/current\n") ;
    }

    /** runas */
    if (!res->owner && scripts->runas)
        auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", res->sa.s + scripts->runas, "\n") ;

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
    int build = !strcmp(res->sa.s + scripts->build, "custom") ? 1 : 0 ;
    size_t execlen = !build ? (res->environ.envdir ? (strlen(res->sa.s + res->environ.envdir) + 14 + SS_SYM_VERSION_LEN + 1) : 0) : 0 ;

    char run[shebanglen + execlen + scriptlen + 4 + 1] ;

    auto_strings(run, "#!") ;

    if (build && scripts->shebang) {

        auto_strings(run + FAKELEN, res->sa.s + scripts->shebang, "\n") ;

    } else {

        auto_strings(run + FAKELEN, shebang) ;
        if (res->environ.envdir)
            auto_strings(run + FAKELEN, "execl-envfile ", res->sa.s + res->environ.envdir, SS_SYM_VERSION "\n") ;
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

    int build = !strcmp(res->sa.s + res->logger.execute.run.build, "custom") ? 1 : 0 ;

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
                    res->sa.s + res->logger.name, "\"\n", \
                    "fdmove -c 2 1\n") ;

        /** runas */
        if (!res->owner)
            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", logrunner, "\n") ;

        auto_strings(run + FAKELEN, "./run.user\n") ;

        res->logger.execute.run.run = resolve_add_string(wres, run) ;

    }

    {
        if (!build) {
            /** run.user script */
            char run[shebanglen + strlen(pback) + strlen(timestamp) + strlen(pmax) + strlen(res->sa.s + res->logger.destination) + 17 + 1] ;

            auto_strings(run, shebang) ;

            auto_strings(run + FAKELEN, "s6-log ") ;

            if (SS_LOGGER_NOTIFY)
                auto_strings(run + FAKELEN, "-d3 ") ;

            auto_strings(run + FAKELEN, "n", pback, " ") ;

            if (res->logger.timestamp < TIME_NONE)
                auto_strings(run + FAKELEN, timestamp, " ") ;

            auto_strings(run + FAKELEN, "s", pmax, " ", res->sa.s + res->logger.destination, "\n") ;

            res->logger.execute.run.run_user = resolve_add_string(wres, run) ;

        } else {

            char *shebang = res->logger.execute.run.shebang ? res->sa.s + res->logger.execute.run.shebang : "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
            size_t shebanglen = strlen(shebang) ;

            char run[shebanglen + strlen(res->sa.s + res->logger.execute.run.run_user) + 2] ;
            auto_strings(run, shebang, res->sa.s + res->logger.execute.run.run_user, "\n") ;

            res->logger.execute.run.run_user = resolve_add_string(wres, run) ;
        }
    }

    free(wres) ;
}

static void compute_log(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    if (!res->logger.name)
        return ;

    resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &lres) ;

    resolve_init(wres) ;

    char *str = res->sa.s ;
    char *name = str + res->logger.name ;
    size_t namelen = strlen(name) ;

    char description[namelen + 7 + 1] ;
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

    lres.path.home = resolve_add_string(wres, str + res->path.home) ;
    lres.path.frontend = resolve_add_string(wres, str + res->path.frontend) ;

    {
        char status[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + strlen(name) + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;
        auto_strings(status, res->sa.s + res->path.home, SS_SYSTEM, SS_SYSTEM, SS_SVC, "/", name, SS_STATE, "/" SS_STATUS) ;
        lres.path.status = resolve_add_string(wres, status) ;

    }

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
    lres.live.scandir = compute_scan_dir(wres, info, name) ;
    lres.live.statedir = compute_state_dir(wres, info, name, "state") ;
    lres.live.eventdir = compute_state_dir(wres, info, name, "event") ;
    lres.live.notifdir = compute_state_dir(wres, info, name, "notif") ;
    lres.live.supervisedir = compute_state_dir(wres, info, name, "supervise") ;
    lres.live.fdholderdir = compute_pipe_service(wres, info, name, SS_FDHOLDER) ;
    lres.live.oneshotddir = compute_pipe_service(wres, info, name, SS_ONESHOTD) ;

    // oneshot do not use fdholder daemon
    if (res->type == TYPE_CLASSIC) {

        compute_log_script(res) ;
        lres.execute.run.run = resolve_add_string(wres, res->sa.s + res->logger.execute.run.run) ;
        lres.execute.run.run_user = resolve_add_string(wres, res->sa.s + res->logger.execute.run.run_user) ;
        lres.execute.run.runas = resolve_add_string(wres, res->sa.s + res->logger.execute.run.runas) ;
    }

    if (!service_resolve_write(&lres))
        log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

    resolve_free(wres) ;
}

void parse_compute_resolve(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char name[strlen(res->sa.s + res->name) + 1] ;
    char status[info->base.len + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + strlen(name) + SS_STATE_LEN + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(name, res->sa.s + res->name) ;
    auto_strings(status, info->base.s, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name, SS_STATE, "/", SS_STATUS) ;

    res->path.status = resolve_add_string(wres, status) ;

    res->path.home = resolve_add_string(wres, info->base.s) ;

    /** Command line precede frontend file which precede the default tree name*/
    if (info->opt_tree)
        res->treename = resolve_add_string(wres, info->treename.s) ;
    else if (res->intree)
        res->treename = resolve_add_string(wres, res->sa.s + res->intree) ;
    else
        res->treename = resolve_add_string(wres, info->treename.s) ;

    /* live */
    res->live.livedir = resolve_add_string(wres, info->live.s) ;

    /* scandir */
    /**
     *
     * /run/66/state/uid/service_name/scandir/service_name
     *
     * Symlink name:
     *
     *      /run/66/scandir/uid/service_name
     *
     * Symlink poiting to:
     *
     *      /run/66/state/uid/service_name
     *
     * which is a symlink pointing to
     *
     *      /var/lib/66/system/treename/servicedirs/svc/service_name
     *
     * which is a symlink pointing to:
     *
     *      /run/66/scandir/uid/
     * */
    res->live.scandir = compute_scan_dir(wres, info, name) ;

    /* state */
    /**
     *
     * see above:
     *
     *      /run/66/state/uid/service_name/state
     *      /run/66/state/uid/service_name/event
     *      /run/66/state/uid/service_name/supervise
     * */
    res->live.statedir = compute_state_dir(wres, info, name, "state") ;

    /* event */
    res->live.eventdir = compute_state_dir(wres, info, name, "event") ;

    /* notif */
    res->live.notifdir = compute_state_dir(wres, info, name, "notif") ;

    /* supervise */
    res->live.supervisedir = compute_state_dir(wres, info, name, "supervise") ;

    /* fdholder */
    res->live.fdholderdir = compute_pipe_service(wres, info, name, SS_FDHOLDER) ;

    /* oneshotd */
    res->live.oneshotddir = compute_pipe_service(wres, info, name, SS_ONESHOTD) ;

    if (res->logger.want && res->type != TYPE_MODULE && res->type != TYPE_BUNDLE) {

        size_t namelen = strlen(name) ;
        char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

        auto_strings(logname, name, SS_LOG_SUFFIX) ;

        res->logger.name = resolve_add_string(wres, logname) ;

        res->logger.destination = compute_log_dir(wres, res) ;

        if (res->type != TYPE_ONESHOT) {

            /** the logger is not a service with oneshot type */
            char buf[strlen(res->sa.s + res->dependencies.depends) + 1 + strlen(res->sa.s + res->logger.name) + 1] ;
            auto_strings(buf, res->sa.s + res->dependencies.depends, " ", res->sa.s + res->logger.name) ;

            res->dependencies.depends = resolve_add_string(wres, buf) ;
            res->dependencies.ndepends++ ;

            compute_log(res, info) ;
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

    if (!service_resolve_write(res))
        log_dieu(LOG_EXIT_SYS, "write resolve file of service: ", name) ;

    free(wres) ;
}
