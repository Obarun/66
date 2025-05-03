/*
 * service_resolve_modify_field.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

static uint32_t resolve_add_uint(char const *data)
{
    uint32_t u ;

    if (!data)
        data = "0" ;
    if (!uint0_scan(data, &u))
        return 0 ;
    return u ;
}

static void modify_config(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_CONFIG_RVERSION:
            res->rversion = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_NAME:
            res->name = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_DESCRIPTION:
            res->description = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_VERSION:
            res->version = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TYPE:
            res->type = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_NOTIFY:
            res->notify = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_MAXDEATH:
            res->maxdeath = resolve_add_uint(data) ; ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_EARLIER:
            res->earlier = resolve_add_uint(data) ; ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_COPYFROM:
            res->copyfrom = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INTREE:
            res->intree = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNERSTR:
            res->ownerstr = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNER:
            res->owner = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TREENAME:
            res->treename = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_USER:
            res->user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INNS:
            res->inns = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ENABLED:
            res->enabled = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ISLOG:
            res->islog = resolve_add_uint(data) ;
            break ;

        default:
            break;
    }

    free(wres) ;
}

static void modify_path(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_PATH_HOME:
        res->path.home = resolve_add_string(wres, data) ;
        break ;

        case E_RESOLVE_SERVICE_PATH_FRONTEND:
            res->path.frontend = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_PATH_SERVICEDIR:
            res->path.servicedir = resolve_add_string(wres, data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_deps(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_DEPS_DEPENDS:
            res->dependencies.depends = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_REQUIREDBY:
            res->dependencies.requiredby = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_OPTSDEPS:
            res->dependencies.optsdeps = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONTENTS:
            res->dependencies.contents = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_PROVIDE:
            res->dependencies.provide = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NDEPENDS:
            res->dependencies.ndepends = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NREQUIREDBY:
            res->dependencies.nrequiredby = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NOPTSDEPS:
            res->dependencies.noptsdeps = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONTENTS:
            res->dependencies.ncontents = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NPROVIDE:
            res->dependencies.nprovide = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_execute(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_EXECUTE_RUN:
            res->execute.run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_USER:
            res->execute.run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD:
            res->execute.run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS:
            res->execute.run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH:
            res->execute.finish.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_USER:
            res->execute.finish.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD:
            res->execute.finish.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS:
            res->execute.finish.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART:
            res->execute.timeout.start = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP:
            res->execute.timeout.stop = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWN:
            res->execute.down = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL:
            res->execute.downsignal = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_BLOCK_PRIVILEGES:
            res->execute.blockprivileges = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_UMASK:
            res->execute.umask = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_UMASK:
            res->execute.want_umask = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_live(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_LIVE_LIVEDIR:
            res->live.livedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATUS:
            res->live.status = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SERVICEDIR:
            res->live.servicedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SCANDIR:
            res->live.scandir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATEDIR:
            res->live.statedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_EVENTDIR:
            res->live.eventdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_NOTIFDIR:
            res->live.notifdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR:
            res->live.supervisedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR:
            res->live.fdholderdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR:
            res->live.oneshotddir = resolve_add_string(wres, data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_logger(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_LOGGER_LOGNAME:
            res->logger.name = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGBACKUP:
            res->logger.backup = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGMAXSIZE:
            res->logger.maxsize = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMESTAMP:
            res->logger.timestamp = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGWANT:
            res->logger.want = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN:
            res->logger.execute.run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_USER:
            res->logger.execute.run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_BUILD:
            res->logger.execute.run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_RUNAS:
            res->logger.execute.run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTART:
            res->logger.timeout.start = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTOP:
            res->logger.timeout.stop = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_environ(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_ENVIRON_ENV:
            res->environ.env = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENVDIR:
            res->environ.envdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE:
            res->environ.env_overwrite = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE:
            res->environ.importfile = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE:
            res->environ.nimportfile = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;

}

static void modify_regex(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_REGEX_CONFIGURE:
            res->regex.configure = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_DIRECTORIES:
            res->regex.directories = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_FILES:
            res->regex.files = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_INFILES:
            res->regex.infiles = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NDIRECTORIES:
            res->regex.ndirectories = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NFILES:
            res->regex.nfiles = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NINFILES:
            res->regex.ninfiles = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_io(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_IO_STDIN:
            res->io.fdin.type = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDINDEST:
            res->io.fdin.destination = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUT:
            res->io.fdout.type = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUTDEST:
            res->io.fdout.destination = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERR:
            res->io.fderr.type = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERRDEST:
            res->io.fderr.destination = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

static void modify_limit(resolve_service_t *res, char const *data, uint32_t field)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch (field) {

        case E_RESOLVE_SERVICE_LIMIT_AS:
            res->limit.limitas = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_CORE:
            res->limit.limitcore = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_CPU:
            res->limit.limitcpu = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_DATA:
            res->limit.limitdata = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_FSIZE:
            res->limit.limitfsize = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_LOCKS:
            res->limit.limitlocks = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_MEMLOCK:
            res->limit.limitmemlock = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_MSGQUEUE:
            res->limit.limitmsgqueue = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NICE:
            res->limit.limitnice = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NOFILE:
            res->limit.limitnofile = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NPROC:
            res->limit.limitnproc = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_RTPRIO:
            res->limit.limitrtprio = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_RTTIME:
            res->limit.limitrttime = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_SIGPENDING:
            res->limit.limitsigpending = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_STACK:
            res->limit.limitstack = resolve_add_uint(data) ;
            break ;

        default:
            break ;
    }

    free(wres) ;
}

void service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_table_t table, char const *data)
{
    log_flow() ;

    switch(table.category) {

        case E_RESOLVE_SERVICE_CATEGORY_CONFIG:
            modify_config(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_PATH:
            modify_path(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_DEPS:
            modify_deps(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_EXECUTE:
            modify_execute(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_LIVE:
            modify_live(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_LOGGER:
            modify_logger(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_ENVIRON:
            modify_environ(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_REGEX:
            modify_regex(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_IO:
            modify_io(res, data, table.id) ;
            break ;

        case E_RESOLVE_SERVICE_CATEGORY_LIMIT:
            modify_limit(res, data, table.id) ;
            break ;

        default:
            break ;
    }

    service_resolve_sanitize(res) ;
}
