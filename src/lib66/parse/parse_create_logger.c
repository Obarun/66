/*
 * parse_create_logger.c
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

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/parse.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

uint32_t compute_log_dir(resolve_wrapper_t_ref wres, resolve_service_t *res, const char *destination)
{
    log_flow() ;

    size_t namelen = strlen(res->sa.s + res->name) ;
    size_t syslen = res->owner ? strlen(res->sa.s + res->path.home) + strlen(SS_LOGGER_USERDIR) : strlen(SS_LOGGER_SYSDIR) ;
    size_t dstlen = destination ? strlen(destination) : strlen(SS_LOGGER_SYSDIR) ;

    char dstlog[syslen + dstlen + namelen + 1] ;

    if (!destination) {

        if (res->owner) {

            char home[syslen + 1 + strlen(SS_LOGGER_USERDIR) + 1] ;

            if (!set_ownerhome_stack(home))
                log_dieusys(LOG_EXIT_SYS,"set home directory") ;

            auto_strings(dstlog, home, SS_LOGGER_USERDIR, res->sa.s + res->name) ;

        } else
            auto_strings(dstlog, SS_LOGGER_SYSDIR, res->sa.s + res->name) ;

    } else {

        auto_strings(dstlog, destination) ;
    }

    return resolve_add_string(wres, dstlog) ;
}

static void compute_log_script(resolve_service_t *log, resolve_service_addon_execute_t *logexec, resolve_service_addon_io_t *io, resolve_service_addon_logger_t *lg)
{

    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, logexec) ;

    int build = !strcmp(lg->sa.s + lg->execute.run.build, "custom") ? E_PARSER_BUILD_CUSTOM : E_PARSER_BUILD_AUTO ;

    char *pmax = 0 ;
    char *pback = 0 ;
    char max[U32_FMT] ;
    char back[U32_FMT] ;
    char *timestamp = 0 ;
    int itimestamp = SS_LOGGER_TIMESTAMP ;
    char *logrunner = lg->execute.run.runas ? lg->sa.s + lg->execute.run.runas : SS_LOGGER_RUNNER ;

    logexec->run.runas = resolve_add_string(wres, logrunner) ;

    // timestamp
    uint32_t ts = lg->timestamp == E_PARSER_TIME_ENDOFKEY ? (uint32_t)itimestamp : lg->timestamp ;
    timestamp = ts == E_PARSER_TIME_NONE ? "" : ts == E_PARSER_TIME_ISO ? "T" : "t" ;

    /** backup */
    if (lg->backup) {

        back[u32_fmt(back,lg->backup)] = 0 ;
        pback = back ;

    }

    /** file size */
    if (lg->maxsize) {

        max[u32_fmt(max,lg->maxsize)] = 0 ;
        pmax = max ;

    }

    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;

    {
        /** run scripts */
        char run[strlen(shebang) + strlen(SS_EXTLIBEXECPREFIX) + 17 + strlen(log->sa.s + log->name) + 1 + 1] ;

        auto_strings(run, \
                    shebang, \
                    SS_EXTLIBEXECPREFIX "66-execute start ", \
                    log->sa.s + log->name, "\n") ;

        logexec->run.run = resolve_add_string(wres, run) ;

    }

    {
        if (!build) {
            /** run.user script */
            char run[SS_MAX_PATH_LEN + 1] ;

            auto_strings(run, shebang) ;

            auto_strings(run + FAKELEN, SS_BINPREFIX "66-log ") ;

            if (SS_LOGGER_NOTIFY)
                auto_strings(run + FAKELEN, "-d3 ") ;

            auto_strings(run + FAKELEN, "n", pback, " ") ;

            if (timestamp[0])
                auto_strings(run + FAKELEN, timestamp, " ") ;

            auto_strings(run + FAKELEN, "s", pmax, " ", io->sa.s + io->fdout.destination, "\n") ;

            logexec->run.run_user = resolve_add_string(wres, run) ;

        } else {

            char run[strlen(lg->sa.s + lg->execute.run.run_user) + 2] ;
            auto_strings(run, lg->sa.s + lg->execute.run.run_user, "\n") ;

            logexec->run.run_user = resolve_add_string(wres, run) ;
        }
    }

    free(wres) ;
}

static void compute_logger(resolve_service_t *res, resolve_service_t *log, resolve_service_addon_io_t *io, resolve_service_addon_io_t *logio, resolve_service_addon_logger_t *lg, resolve_service_addon_execute_t *parentexec, resolve_service_addon_execute_t *logexec, resolve_service_addon_dependencies_t *logdep, ssexec_t *info)
{
    log_flow() ;

    if (!res->logger)
        return ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, log) ;
    resolve_wrapper_t_ref exwres = resolve_set_struct(DATA_SERVICE_EXECUTE, logexec) ;

    resolve_init(wres) ;

    char *str = res->sa.s ;
    size_t pnamelen = strlen(str + res->name) ;
    char name[pnamelen + SS_LOG_SUFFIX_LEN + 1] ; // logger name is always <service>-log
    char description[pnamelen + 7 + 1] ;

    auto_strings(name, str + res->name, SS_LOG_SUFFIX) ;

    auto_strings(description, str + res->name, " logger") ;

    log->name = resolve_add_string(wres, name) ;
    log->description = resolve_add_string(wres, description) ;
    log->version = resolve_add_string(wres, str + res->version) ;
    log->type = res->type ;
    logexec->notify = 3 ;
    logexec->maxdeath = parentexec->maxdeath ;
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
    {
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, logdep) ;
        logdep->requiredby = resolve_add_string(depwres, str + res->name) ;
        logdep->nrequiredby = 1 ;
        free(depwres) ;
    }
    log->has_dependencies = 1 ;
    logexec->run.build = lg->execute.run.build ? resolve_add_string(exwres, lg->sa.s + lg->execute.run.build) : 0 ;
    logexec->run.runas = resolve_add_string(exwres, lg->sa.s + lg->execute.run.runas) ;
    logexec->timeout.start = lg->execute.timeout.start ;
    logexec->timeout.stop = lg->execute.timeout.stop ;
    logexec->down = lg->execute.down ;
    logexec->downsignal = lg->execute.downsignal ;

    log->live.livedir = resolve_add_string(wres, info->live.s) ;
    log->live.status = compute_status(wres, info) ;
    log->live.servicedir = compute_live_servicedir(wres, info) ;
    log->live.scandir = compute_scan_dir(wres, info) ;
    log->live.statedir = compute_state_dir(wres, info, SS_STATE + 1) ;
    log->live.eventdir = compute_state_dir(wres, info, SS_EVENTDIR + 1) ;
    log->live.supervisedir = compute_state_dir(wres, info, SS_SUPERVISEDIR + 1) ;
    log->live.fdholderdir = compute_pipe_service(wres, info, SS_FDHOLDER) ;
    log->live.oneshotddir = compute_pipe_service(wres, info, SS_ONESHOTD) ;

    {
        resolve_wrapper_t_ref iowres = resolve_set_struct(DATA_SERVICE_IO, logio) ;
        resolve_init(iowres) ;

        if (!strcmp(lg->sa.s + lg->execute.run.build, "custom")) {

            logio->fdin.type = logio->fdout.type = logio->fderr.type = E_PARSER_IO_TYPE_PARENT ;

        } else {

            logio->fdin.type = logio->fdout.type = E_PARSER_IO_TYPE_66LOG ;
            logio->fdin.destination = resolve_add_string(iowres, res->sa.s + res->live.fdholderdir) ;
            logio->fdout.destination = logio->fderr.destination = resolve_add_string(iowres, io->sa.s + io->fdout.destination) ;
            logio->fderr.type = E_PARSER_IO_TYPE_INHERIT ;
        }

        free(iowres) ;
    }
    log->has_io = 1 ;
    log->has_execute = 1 ;

    // oneshot do not use fdholder daemon
    if (res->type == E_PARSER_TYPE_CLASSIC)
        compute_log_script(log, logexec, io, lg) ;

    free(exwres) ;
    free(wres) ;

}

void parse_create_logger(hash_t *hres, struct resolve_hash_s *c, ssexec_t *info)
{
    log_flow() ;

    resolve_service_t *res = &c->res ;
    resolve_service_addon_io_t *io = &c->io ;
    resolve_service_addon_logger_t *lg = &c->logger ;
    resolve_service_addon_execute_t *parentexec = &c->execute ;
    resolve_service_addon_dependencies_t *parentdep = &c->dependencies ;
    char logname[strlen(res->sa.s + res->name) + SS_LOG_SUFFIX_LEN + 1] ;
    auto_strings(logname, res->sa.s + res->name, SS_LOG_SUFFIX) ;

    struct resolve_hash_s *hash ;
    resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_io_t logio = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_service_addon_execute_t logexec = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_service_addon_dependencies_t logdep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    {
        resolve_wrapper_t_ref exwres = resolve_set_struct(DATA_SERVICE_EXECUTE, &logexec) ;
        resolve_init(exwres) ;
        free(exwres) ;
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &logdep) ;
        resolve_init(depwres) ;
        free(depwres) ;
    }

    hash = resolve_hash_search(hres, logname) ;
    if (hash == NULL && res->type == E_PARSER_TYPE_CLASSIC) {
        /** the logger is not a service with oneshot type */

        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, parentdep) ;

        if (parentdep->ndepends) {

            char buf[strlen(parentdep->sa.s + parentdep->depends) + 1 + strlen(logname) + 1] ;
            auto_strings(buf, parentdep->sa.s + parentdep->depends, " ", logname) ;

            parentdep->depends = resolve_add_string(depwres, buf) ;

        } else {

            parentdep->depends = resolve_add_string(depwres, logname) ;
        }

        parentdep->ndepends++ ;

        free(depwres) ;

        compute_logger(res, &lres, io, &logio, lg, parentexec, &logexec, &logdep, info) ;

        /** keep the derived run scripts on the parent's logger addon (dump/reference) */
        {
            resolve_wrapper_t_ref lgwres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;
            lg->execute.run.run = resolve_add_string(lgwres, logexec.sa.s + logexec.run.run) ;
            lg->execute.run.run_user = resolve_add_string(lgwres, logexec.sa.s + logexec.run.run_user) ;
            free(lgwres) ;
        }

        if (resolve_hash_count(hres) > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

        log_trace("add service: ", logname, " to the service selection") ;
        if (!resolve_hash_add(hres, logname, lres))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", logname) ;

        hash = resolve_hash_search(hres, logname) ;
        hash->io = logio ;
        hash->dependencies = logdep ;
        hash->execute = logexec ;
    }

    free(wres) ;
}
