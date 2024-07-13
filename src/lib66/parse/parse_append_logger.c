/*
 * parse_append_logger.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/parse.h>

#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

uint32_t compute_log_dir(resolve_wrapper_t_ref wres, resolve_service_t *res)
{
    log_flow() ;

    size_t namelen = strlen(res->sa.s + res->name) ;
    size_t syslen = res->owner ? strlen(res->sa.s + res->path.home) + strlen(SS_LOGGER_USERDIR) : strlen(SS_LOGGER_SYSDIR) ;
    size_t dstlen = res->logger.destination ? strlen(res->sa.s + res->logger.destination) : strlen(SS_LOGGER_SYSDIR) ;

    char dstlog[syslen + dstlen + namelen + 1] ;

    if (!res->logger.destination) {

        if (res->owner) {

            char home[syslen + 1 + strlen(SS_LOGGER_USERDIR) + 1] ;

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


static void compute_log_script(resolve_service_t *res, resolve_service_t *log)
{

    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, log) ;

    int build = !strcmp(res->sa.s + res->logger.execute.run.build, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;

    char *pmax = 0 ;
    char *pback = 0 ;
    char max[UINT32_FMT] ;
    char back[UINT32_FMT] ;
    char *timestamp = 0 ;
    int itimestamp = SS_LOGGER_TIMESTAMP ;
    char *logrunner = res->logger.execute.run.runas ? res->sa.s + res->logger.execute.run.runas : SS_LOGGER_RUNNER ;

    log->execute.run.runas = resolve_add_string(wres, logrunner) ;

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

    {
        /** run scripts */
        char run[strlen(shebang) + 17 + strlen(res->sa.s + res->name) + 1 + 1] ;

        auto_strings(run, \
                    shebang, \
                    SS_EXTLIBEXECPREFIX "66-execute start ", \
                    log->sa.s + log->name, "\n") ;

        log->execute.run.run = resolve_add_string(wres, run) ;

    }

    {
        if (!build) {
            /** run.user script */
            char run[SS_MAX_PATH_LEN + 1] ;

            auto_strings(run, shebang) ;

            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-log ") ;

            if (SS_LOGGER_NOTIFY)
                auto_strings(run + FAKELEN, "-d3 ") ;

            auto_strings(run + FAKELEN, "n", pback, " ") ;

            if (res->logger.timestamp < TIME_NONE)
                auto_strings(run + FAKELEN, timestamp, " ") ;

            auto_strings(run + FAKELEN, "s", pmax, " ", res->sa.s + res->logger.destination, "\n") ;

            log->execute.run.run_user = resolve_add_string(wres, run) ;

        } else {

            char run[strlen(res->sa.s + res->logger.execute.run.run_user) + 2] ;
            auto_strings(run, res->sa.s + res->logger.execute.run.run_user, "\n") ;

            log->execute.run.run_user = resolve_add_string(wres, run) ;
        }
    }

    free(wres) ;
}

static void compute_logger(resolve_service_t *res, resolve_service_t *log, ssexec_t *info)
{
    log_flow() ;

    if (!res->logger.name)
        return ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, log) ;

    resolve_init(wres) ;

    char *str = res->sa.s ;
    size_t namelen = strlen(str + res->logger.name) ;
    char name[namelen + 1] ;
    char description[namelen + 7 + 1] ;

    auto_strings(name, str + res->logger.name) ;

    auto_strings(description, str + res->name, " logger") ;

    log->name = resolve_add_string(wres, name) ;
    log->description = resolve_add_string(wres, description) ;
    log->version = resolve_add_string(wres, str + res->version) ;
    log->type = res->type ;
    log->notify = 3 ;
    log->maxdeath = res->maxdeath ;
    log->earlier = res->earlier ;
    if (res->intree)
        log->intree = resolve_add_string(wres, str + res->intree) ;

    log->ownerstr = resolve_add_string(wres, str + res->ownerstr) ;
    log->owner = res->owner ;
    log->treename = resolve_add_string(wres, str + res->treename) ;
    log->user = resolve_add_string(wres, str + res->user) ;
    if (res->inns)
        log->inns = resolve_add_string(wres, str + res->inns) ;
    log->islog = 1 ;

    log->path.home = resolve_add_string(wres, str + res->path.home) ;
    log->path.frontend = resolve_add_string(wres, str + res->path.frontend) ;
    log->path.servicedir = compute_src_servicedir(wres, info) ;
    log->dependencies.requiredby = resolve_add_string(wres, str + res->name) ;
    log->dependencies.nrequiredby = 1 ;
    log->execute.run.build = res->logger.execute.run.build ? resolve_add_string(wres, str + res->logger.execute.run.build) : 0 ;
    log->execute.run.runas = resolve_add_string(wres, str + res->logger.execute.run.runas) ;
    log->execute.timeout.start = res->logger.execute.timeout.start ;
    log->execute.timeout.stop = res->logger.execute.timeout.stop ;
    log->execute.down = res->logger.execute.down ;
    log->execute.downsignal = res->logger.execute.downsignal ;

    log->live.livedir = resolve_add_string(wres, info->live.s) ;
    log->live.status = compute_status(wres, info) ;
    log->live.servicedir = compute_live_servicedir(wres, info) ;
    log->live.scandir = compute_scan_dir(wres, info) ;
    log->live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;
    log->live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;
    log->live.notifdir = compute_state_dir(wres, info, "notif") ;
    log->live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;
    log->live.fdholderdir = compute_pipe_service(wres, info, SS_FDHOLDER) ;
    log->live.oneshotddir = compute_pipe_service(wres, info, SS_ONESHOTD) ;

    log->logger.destination = resolve_add_string(wres, str + res->logger.destination) ;
    log->logger.backup = res->logger.backup ;
    log->logger.maxsize = res->logger.maxsize ;
    log->logger.timestamp = res->logger.timestamp ;
    log->logger.want = 0 ;

    if (!strcmp(res->sa.s + res->logger.execute.run.build, "custom")) {

        log->io.fdin.type = log->io.fdout.type = log->io.fderr.type = IO_TYPE_PARENT ;

    } else {

        log->io.fdin.type = log->io.fdout.type = IO_TYPE_S6LOG ;
        log->io.fdin.destination = log->io.fdout.destination = compute_log_dir(wres, res) ;
        log->io.fderr.type = IO_TYPE_INHERIT ;
    }

    // oneshot do not use fdholder daemon
    if (res->type == TYPE_CLASSIC)
        compute_log_script(res, log) ;

    free(wres) ;

}

void parse_append_logger(struct resolve_hash_s **hres, resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    char *logname = res->sa.s + res->logger.name ;
    struct resolve_hash_s *hash ;
    resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    hash = hash_search(hres, logname) ;
    if (hash == NULL && res->type == TYPE_CLASSIC) {
        /** the logger is not a service with oneshot type */

        if (res->dependencies.ndepends) {

            char buf[strlen(res->sa.s + res->dependencies.depends) + 1 + strlen(logname) + 1] ;
            auto_strings(buf, res->sa.s + res->dependencies.depends, " ", logname) ;

            res->dependencies.depends = resolve_add_string(wres, buf) ;

        } else {

            res->dependencies.depends = resolve_add_string(wres, logname) ;
        }

        res->dependencies.ndepends++ ;

        compute_logger(res, &lres, info) ;

        /** sanitize_init/execute_uidgid use this field */
        res->logger.execute.run.run = resolve_add_string(wres, lres.sa.s + lres.execute.run.run) ;
        res->logger.execute.run.run_user = resolve_add_string(wres, lres.sa.s + lres.execute.run.run_user) ;

        if (hash_count(hres) > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

        log_trace("add service: ", logname, " to the service selection") ;
        if (!hash_add(hres, logname, lres))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", logname) ;
    }

    free(wres) ;
}
