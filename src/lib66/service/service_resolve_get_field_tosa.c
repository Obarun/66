/*
 * service_resolve_get_field_tosa.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_get_field_tosa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field)
{
    log_flow() ;

    char fmt[UINT32_FMT] ;
    char const *str = 0 ;
    int e = 0 ;

    switch(field) {

        // configuration

        case E_RESOLVE_SERVICE_NAME:
            str = res->sa.s + res->name ;
            break ;

        case E_RESOLVE_SERVICE_DESCRIPTION:
            str = res->sa.s + res->description ;
            break ;

        case E_RESOLVE_SERVICE_VERSION:
            str = res->sa.s + res->version ;
            break ;

        case E_RESOLVE_SERVICE_TYPE:
            fmt[uint32_fmt(fmt,res->type)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_NOTIFY:
            fmt[uint32_fmt(fmt,res->notify)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_MAXDEATH:
            fmt[uint32_fmt(fmt,res->maxdeath)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_EARLIER:
            fmt[uint32_fmt(fmt,res->earlier)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_HIERCOPY:
            str = res->sa.s + res->hiercopy ;
            break ;

        case E_RESOLVE_SERVICE_INTREE:
            str = res->sa.s + res->intree ;
            break ;

        case E_RESOLVE_SERVICE_OWNERSTR:
            str = res->sa.s + res->ownerstr ;
            break ;

        case E_RESOLVE_SERVICE_OWNER:
            fmt[uint32_fmt(fmt,res->owner)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_TREENAME:
            str = res->sa.s + res->treename ;
            break ;

        case E_RESOLVE_SERVICE_USER:
            str = res->sa.s + res->user ;
            break ;

        case E_RESOLVE_SERVICE_INMODULE:
            str = res->sa.s + res->inmodule ;
            break ;

        // path configuration

        case E_RESOLVE_SERVICE_HOME:
            str = res->sa.s + res->path.home ;
            break ;

        case E_RESOLVE_SERVICE_FRONTEND:
            str = res->sa.s + res->path.frontend ;
            break ;

        case E_RESOLVE_SERVICE_STATUS:
            str = res->sa.s + res->path.status ;

        case E_RESOLVE_SERVICE_SERVICEDIR:
            str = res->sa.s + res->path.servicedir ;

        // dependencies

        case E_RESOLVE_SERVICE_DEPENDS:
            str = res->sa.s + res->dependencies.depends ;
            break ;

        case E_RESOLVE_SERVICE_REQUIREDBY:
            str = res->sa.s + res->dependencies.requiredby ;
            break ;

        case E_RESOLVE_SERVICE_OPTSDEPS:
            str = res->sa.s + res->dependencies.optsdeps ;
            break ;

        case E_RESOLVE_SERVICE_CONTENTS:
            str = res->sa.s + res->dependencies.contents ;
            break ;

        case E_RESOLVE_SERVICE_NDEPENDS:
            fmt[uint32_fmt(fmt,res->dependencies.ndepends)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_NREQUIREDBY:
            fmt[uint32_fmt(fmt,res->dependencies.nrequiredby)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_NOPTSDEPS:
            fmt[uint32_fmt(fmt,res->dependencies.noptsdeps)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_NCONTENTS:
            fmt[uint32_fmt(fmt,res->dependencies.ncontents)] = 0 ;
            str = fmt ;
            break ;

        // execute

        case E_RESOLVE_SERVICE_RUN:
            str = res->sa.s + res->execute.run.run ;
            break ;

        case E_RESOLVE_SERVICE_RUN_USER:
            str = res->sa.s + res->execute.run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_RUN_BUILD:
            str = res->sa.s + res->execute.run.build ;
            break ;

        case E_RESOLVE_SERVICE_RUN_SHEBANG:
            str = res->sa.s + res->execute.run.shebang ;
            break ;

        case E_RESOLVE_SERVICE_RUN_RUNAS:
            str = res->sa.s + res->execute.run.runas ;
            break ;

        case E_RESOLVE_SERVICE_FINISH:
            str = res->sa.s + res->execute.finish.run ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_USER:
            str = res->sa.s + res->execute.finish.run_user ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_BUILD:
            str = res->sa.s + res->execute.finish.build ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_SHEBANG:
            str = res->sa.s + res->execute.finish.shebang ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_RUNAS:
            str = res->sa.s + res->execute.finish.runas ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTKILL:
            fmt[uint32_fmt(fmt,res->execute.timeout.kill)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTFINISH:
            fmt[uint32_fmt(fmt,res->execute.timeout.finish)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTUP:
            fmt[uint32_fmt(fmt,res->execute.timeout.up)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTDOWN:
            fmt[uint32_fmt(fmt,res->execute.timeout.down)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DOWN:
            fmt[uint32_fmt(fmt,res->execute.down)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_DOWNSIGNAL:
            fmt[uint32_fmt(fmt,res->execute.downsignal)] = 0 ;
            str = fmt ;
            break ;

        // live

        case E_RESOLVE_SERVICE_LIVEDIR:
            str = res->sa.s + res->live.livedir ;
            break ;

        case E_RESOLVE_SERVICE_SCANDIR:
            str = res->sa.s + res->live.scandir ;
            break ;

        case E_RESOLVE_SERVICE_STATEDIR:
            str = res->sa.s + res->live.statedir ;
            break ;

        case E_RESOLVE_SERVICE_EVENTDIR:
            str = res->sa.s + res->live.eventdir ;
            break ;

        case E_RESOLVE_SERVICE_NOTIFDIR:
            str = res->sa.s + res->live.notifdir ;
            break ;

        case E_RESOLVE_SERVICE_SUPERVISEDIR:
            str = res->sa.s + res->live.supervisedir ;
            break ;

        case E_RESOLVE_SERVICE_FDHOLDERDIR:
            str = res->sa.s + res->live.fdholderdir ;
            break ;

        case E_RESOLVE_SERVICE_ONESHOTDDIR:
            str = res->sa.s + res->live.oneshotddir ;
            break ;

        // logger

        case E_RESOLVE_SERVICE_LOGNAME:
            str = res->sa.s + res->logger.name ;
            break ;

        case E_RESOLVE_SERVICE_LOGDESTINATION:
            str = res->sa.s + res->logger.destination ;
            break ;

        case E_RESOLVE_SERVICE_LOGBACKUP:
            fmt[uint32_fmt(fmt,res->logger.backup)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGMAXSIZE:
            fmt[uint32_fmt(fmt,res->logger.maxsize)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMESTAMP:
            fmt[uint32_fmt(fmt,res->logger.timestamp)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGWANT:
            fmt[uint32_fmt(fmt,res->logger.want)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN:
            str = res->sa.s + res->logger.execute.run.run ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_USER:
            str = res->sa.s + res->logger.execute.run.run_user ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_BUILD:
            str = res->sa.s + res->logger.execute.run.build ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_SHEBANG:
            str = res->sa.s + res->logger.execute.run.shebang ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_RUNAS:
            str = res->sa.s + res->logger.execute.run.runas ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMEOUTKILL:
            fmt[uint32_fmt(fmt,res->logger.timeout.kill)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMEOUTFINISH:
            fmt[uint32_fmt(fmt,res->logger.timeout.finish)] = 0 ;
            str = fmt ;
            break ;


        // environment

        case E_RESOLVE_SERVICE_ENV:
            str = res->sa.s + res->environ.env ;
            break ;

        case E_RESOLVE_SERVICE_ENVDIR:
            str = res->sa.s + res->environ.envdir ;
            break ;

        case E_RESOLVE_SERVICE_ENV_OVERWRITE:
            fmt[uint32_fmt(fmt,res->environ.env_overwrite)] = 0 ;
            str = fmt ;
            break ;

        // regex

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
            fmt[uint32_fmt(fmt,res->regex.ndirectories)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NFILES:
            fmt[uint32_fmt(fmt,res->regex.nfiles)] = 0 ;
            str = fmt ;
            break ;

        case E_RESOLVE_SERVICE_REGEX_NINFILES:
            fmt[uint32_fmt(fmt,res->regex.ninfiles)] = 0 ;
            str = fmt ;
            break ;

        default:
            return e ;
    }

    if (!auto_stra(sa,str))
        return e ;

    e = 1 ;
    return e ;
}
