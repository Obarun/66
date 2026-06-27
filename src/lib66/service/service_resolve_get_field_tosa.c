/*
 * service_resolve_get_table_tosa.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_service.h>

static int get_config(strbuf *sa, resolve_service_t *res, resolve_service_enum_config_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_CONFIG_RVERSION:
            fmt[u32_fmt(fmt,res->rversion)] = 0 ;
            str = fmt ;
        break ;

        case E_RESOLVE_SERVICE_CONFIG_NAME:
            str = res->sa.s + res->name ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_DESCRIPTION:
            str = res->sa.s + res->description ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_VERSION:
            str = res->sa.s + res->version ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TYPE:
            fmt[u32_fmt(fmt,res->type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_NOTIFY:
            fmt[u32_fmt(fmt,res->notify)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_MAXDEATH:
            fmt[u32_fmt(fmt,res->maxdeath)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_MAXDEATHTIME:
            fmt[u32_fmt(fmt,res->maxdeathtime)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_EARLIER:
            fmt[u32_fmt(fmt,res->earlier)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_COPYFROM:
            str = res->sa.s + res->copyfrom ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INTREE:
            str = res->sa.s + res->intree ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNERSTR:
            str = res->sa.s + res->ownerstr ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_OWNER:
            fmt[u32_fmt(fmt,res->owner)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_TREENAME:
            str = res->sa.s + res->treename ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_USER:
            str = res->sa.s + res->user ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_INNS:
            str = res->sa.s + res->inns ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ENABLED:
            fmt[u32_fmt(fmt,res->enabled)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_CONFIG_ISLOG:
            fmt[u32_fmt(fmt,res->islog)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_path(strbuf *sa, resolve_service_t *res, resolve_service_enum_path_t table)
{
    log_flow() ;

    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_PATH_HOME:
            str = res->sa.s + res->path.home ;
            break ;

        case E_RESOLVE_SERVICE_PATH_FRONTEND:
            str = res->sa.s + res->path.frontend ;
            break ;

        case E_RESOLVE_SERVICE_PATH_SERVICEDIR:
            str = res->sa.s + res->path.servicedir ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_deps(strbuf *sa, resolve_service_t *res, resolve_service_enum_deps_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_DEPS_DEPENDS:
            str = res->sa.s + res->dependencies.depends ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_REQUIREDBY:
            str = res->sa.s + res->dependencies.requiredby ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_OPTSDEPS:
            str = res->sa.s + res->dependencies.optsdeps ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONTENTS:
            str = res->sa.s + res->dependencies.contents ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_PROVIDE:
            str = res->sa.s + res->dependencies.provide ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_CONFLICT:
            str = res->sa.s + res->dependencies.conflict ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NDEPENDS:
            fmt[u32_fmt(fmt,res->dependencies.ndepends)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NREQUIREDBY:
            fmt[u32_fmt(fmt,res->dependencies.nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NOPTSDEPS:
            fmt[u32_fmt(fmt,res->dependencies.noptsdeps)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONTENTS:
            fmt[u32_fmt(fmt,res->dependencies.ncontents)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NPROVIDE:
            fmt[u32_fmt(fmt,res->dependencies.nprovide)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DEPS_NCONFLICT:
            fmt[u32_fmt(fmt,res->dependencies.nconflict)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_exec(strbuf *sa, resolve_service_t *res, resolve_service_enum_execute_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_EXECUTE_RUN:
            str = res->sa.s + res->execute.run.run ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_USER:
            str = res->sa.s + res->execute.run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD:
            str = res->sa.s + res->execute.run.build ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS:
            str = res->sa.s + res->execute.run.runas ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH:
            str = res->sa.s + res->execute.finish.run ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_USER:
            str = res->sa.s + res->execute.finish.run_user ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD:
            str = res->sa.s + res->execute.finish.build ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS:
            str = res->sa.s + res->execute.finish.runas ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART:
            fmt[u32_fmt(fmt,res->execute.timeout.start)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP:
            fmt[u32_fmt(fmt,res->execute.timeout.stop)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWN:
            fmt[u32_fmt(fmt,res->execute.down)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL:
            fmt[u32_fmt(fmt,res->execute.downsignal)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_BLOCK_PRIVILEGES:
            fmt[u32_fmt(fmt,res->execute.blockprivileges)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_UMASK:
            fmt[u32_fmt(fmt,res->execute.umask)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_UMASK:
            fmt[u32_fmt(fmt,res->execute.want_umask)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_NICE:
            fmt[u32_fmt(fmt,res->execute.nice)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_WANT_NICE:
            fmt[u32_fmt(fmt,res->execute.want_nice)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CHDIR:
            str = res->sa.s + res->execute.chdir ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_BOUND:
            str = res->sa.s + res->execute.capsbound ;
            break ;

        case E_RESOLVE_SERVICE_EXECUTE_CAPS_AMBIENT:
            str = res->sa.s + res->execute.capsambient ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_live(strbuf *sa, resolve_service_t *res, resolve_service_enum_live_t table)
{
    log_flow() ;

    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_LIVE_LIVEDIR:
            str = res->sa.s + res->live.livedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATUS:
            str = res->sa.s + res->live.status ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SERVICEDIR:
            str = res->sa.s + res->live.servicedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SCANDIR:
            str = res->sa.s + res->live.scandir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_STATEDIR:
            str = res->sa.s + res->live.statedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_EVENTDIR:
            str = res->sa.s + res->live.eventdir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_NOTIFDIR:
            str = res->sa.s + res->live.notifdir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR:
            str = res->sa.s + res->live.supervisedir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR:
            str = res->sa.s + res->live.fdholderdir ;
            break ;

        case E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR:
            str = res->sa.s + res->live.oneshotddir ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_logger(strbuf *sa, resolve_service_t *res, resolve_service_enum_logger_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_LOGGER_LOGNAME:
            str = res->sa.s + res->logger.name ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGBACKUP:
            fmt[u32_fmt(fmt,res->logger.backup)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGMAXSIZE:
            fmt[u32_fmt(fmt,res->logger.maxsize)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMESTAMP:
            fmt[u32_fmt(fmt,res->logger.timestamp)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGWANT:
            fmt[u32_fmt(fmt,res->logger.want)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN:
            str = res->sa.s + res->logger.execute.run.run ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_USER:
            str = res->sa.s + res->logger.execute.run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_BUILD:
            str = res->sa.s + res->logger.execute.run.build ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGRUN_RUNAS:
            str = res->sa.s + res->logger.execute.run.runas ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTART:
            fmt[u32_fmt(fmt,res->logger.execute.timeout.start)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTOP:
            fmt[u32_fmt(fmt,res->logger.execute.timeout.stop)] = 0 ;
            str = fmt ;
            break ;
        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_environ(strbuf *sa, resolve_service_t *res, resolve_service_enum_environ_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_ENVIRON_ENV:
        str = res->sa.s + res->environ.env ;
        break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENVDIR:
            str = res->sa.s + res->environ.envdir ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE:
            fmt[u32_fmt(fmt,res->environ.env_overwrite)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE:
            str = res->sa.s + res->environ.importfile ;
            break ;

        case E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE:
            fmt[u32_fmt(fmt,res->environ.nimportfile)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_regex(strbuf *sa, resolve_service_t *res, resolve_service_enum_regex_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_REGEX_CONFIGURE:
            str = res->sa.s + res->regex.directories ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_DIRECTORIES:
            str = res->sa.s + res->regex.directories ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_FILES:
            str = res->sa.s + res->regex.files ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_INFILES:
            str = res->sa.s + res->regex.infiles ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NDIRECTORIES:
            fmt[u32_fmt(fmt,res->regex.ndirectories)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NFILES:
            fmt[u32_fmt(fmt,res->regex.nfiles)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NINFILES:
            fmt[u32_fmt(fmt,res->regex.ninfiles)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_io(strbuf *sa, resolve_service_t *res, resolve_service_enum_limit_t table)
{
    log_flow() ;

    char fmt[U32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_IO_STDIN:
            fmt[u32_fmt(fmt,res->io.fdin.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDINDEST:
            str = res->sa.s + res->io.fdin.destination ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUT:
            fmt[u32_fmt(fmt,res->io.fdout.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDOUTDEST:
            str = res->sa.s + res->io.fdout.destination ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERR:
            fmt[u32_fmt(fmt,res->io.fderr.type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_IO_STDERRDEST:
            str = res->sa.s + res->io.fderr.destination ;
            break ;

        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

static int get_limit(strbuf *sa, resolve_service_t *res, resolve_service_enum_limit_t table)
{
    log_flow() ;

    char fmt[U64_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(table) {

        case E_RESOLVE_SERVICE_LIMIT_AS:
            fmt[u64_fmt(fmt,res->limit.limitas)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_CORE:
            fmt[u64_fmt(fmt,res->limit.limitcore)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_CPU:
            fmt[u64_fmt(fmt,res->limit.limitcpu)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_DATA:
            fmt[u64_fmt(fmt,res->limit.limitdata)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_FSIZE:
            fmt[u64_fmt(fmt,res->limit.limitfsize)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_LOCKS:
            fmt[u64_fmt(fmt,res->limit.limitlocks)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_MEMLOCK:
            fmt[u64_fmt(fmt,res->limit.limitmemlock)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_MSGQUEUE:
            fmt[u64_fmt(fmt,res->limit.limitmsgqueue)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NICE:
            fmt[u64_fmt(fmt,res->limit.limitnice)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NOFILE:
            fmt[u64_fmt(fmt,res->limit.limitnofile)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_NPROC:
            fmt[u64_fmt(fmt,res->limit.limitnproc)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_RTPRIO:
            fmt[u64_fmt(fmt,res->limit.limitrtprio)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_RTTIME:
            fmt[u64_fmt(fmt,res->limit.limitrttime)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_SIGPENDING:
            fmt[u64_fmt(fmt,res->limit.limitsigpending)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LIMIT_STACK:
            fmt[u64_fmt(fmt,res->limit.limitstack)] = 0 ;
            str = fmt ;
            break ;


        default:
            return e ;
    }

    if (!auto_strbuf(sa,str))
        return e ;

    e = 1 ;
    return e ;
}

int service_resolve_get_field_tosa(strbuf *sa, resolve_service_t *res, resolve_service_enum_table_t table)
{
    log_flow() ;

    switch(table.category) {

        case E_RESOLVE_SERVICE_CATEGORY_CONFIG :
            return get_config(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_PATH:
            return get_path(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_DEPS:
            return get_deps(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_EXECUTE:
            return get_exec(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_LIVE:
            return get_live(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_LOGGER:
            return get_logger(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_ENVIRON:
            return get_environ(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_REGEX:
            return get_regex(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_IO:
            return get_io(sa, res, table.id) ;

        case E_RESOLVE_SERVICE_CATEGORY_LIMIT:
            return get_limit(sa, res, table.id) ;

        default:
            return 0 ;
    }

    return 1 ;
}
