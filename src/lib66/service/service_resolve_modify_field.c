/*
 * service_resolve_modify_field.c
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

#include <stdint.h>
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>

resolve_field_table_t resolve_service_field_table[] = {

    [E_RESOLVE_SERVICE_NAME] = { .field = "name" },
    [E_RESOLVE_SERVICE_DESCRIPTION] = { .field = "description" },
    [E_RESOLVE_SERVICE_VERSION] = { .field = "version" },
    [E_RESOLVE_SERVICE_TYPE] = { .field = "type" },
    [E_RESOLVE_SERVICE_NOTIFY] = { .field = "notify" },
    [E_RESOLVE_SERVICE_MAXDEATH] = { .field = "maxdeath" },
    [E_RESOLVE_SERVICE_EARLIER] = { .field = "earlier" },
    [E_RESOLVE_SERVICE_HIERCOPY] = { .field = "hiercopy" },
    [E_RESOLVE_SERVICE_INTREE] = { .field = "intree" },
    [E_RESOLVE_SERVICE_OWNERSTR] = { .field = "ownerstr" },
    [E_RESOLVE_SERVICE_OWNER] = { .field = "owner" },
    [E_RESOLVE_SERVICE_TREENAME] = { .field = "treename" },
    [E_RESOLVE_SERVICE_USER] = { .field = "user" },
    [E_RESOLVE_SERVICE_INNS] = { .field = "inns" },
    [E_RESOLVE_SERVICE_ENABLED] = { .field = "enabled" },

    // path
    [E_RESOLVE_SERVICE_HOME] = { .field = "home" },
    [E_RESOLVE_SERVICE_FRONTEND] = { .field = "frontend" },
    [E_RESOLVE_SERVICE_SERVICEDIR] = { .field = "servicedir" },

    // dependencies
    [E_RESOLVE_SERVICE_DEPENDS] = { .field = "depends" },
    [E_RESOLVE_SERVICE_REQUIREDBY] = { .field = "requiredby" },
    [E_RESOLVE_SERVICE_OPTSDEPS] = { .field = "optsdeps" },
    [E_RESOLVE_SERVICE_CONTENTS] = { .field = "contents" },
    [E_RESOLVE_SERVICE_NDEPENDS] = { .field = "ndepends" },
    [E_RESOLVE_SERVICE_NREQUIREDBY] = { .field = "nrequiredby" },
    [E_RESOLVE_SERVICE_NOPTSDEPS] = { .field = "noptsdeps" },
    [E_RESOLVE_SERVICE_NCONTENTS] = { .field = "ncontents" },

    // execute
    [E_RESOLVE_SERVICE_RUN] = { .field = "run" },
    [E_RESOLVE_SERVICE_RUN_USER] = { .field = "run_user" },
    [E_RESOLVE_SERVICE_RUN_BUILD] = { .field = "run_build" },
    [E_RESOLVE_SERVICE_RUN_SHEBANG] = { .field = "run_shebang" },
    [E_RESOLVE_SERVICE_RUN_RUNAS] = { .field = "run_runas" },
    [E_RESOLVE_SERVICE_FINISH] = { .field = "finish" },
    [E_RESOLVE_SERVICE_FINISH_USER] = { .field = "finish_user" },
    [E_RESOLVE_SERVICE_FINISH_BUILD] = { .field = "finish_build" },
    [E_RESOLVE_SERVICE_FINISH_SHEBANG] = { .field = "finish_shebang" },
    [E_RESOLVE_SERVICE_FINISH_RUNAS] = { .field = "finish_runas" },
    [E_RESOLVE_SERVICE_TIMEOUTSTART] = { .field = "timeoutstart" },
    [E_RESOLVE_SERVICE_TIMEOUTSTOP] = { .field = "timeoutstop" },
    [E_RESOLVE_SERVICE_DOWN] = { .field = "down" },
    [E_RESOLVE_SERVICE_DOWNSIGNAL] = { .field = "downsignal" },

    // live
    [E_RESOLVE_SERVICE_LIVEDIR] = { .field = "livedir" },
    [E_RESOLVE_SERVICE_STATUS] = { .field = "status" },
    [E_RESOLVE_SERVICE_SERVICEDIR_LIVE] = { .field = "servicedir" },
    [E_RESOLVE_SERVICE_SCANDIR] = { .field = "scandirdir" },
    [E_RESOLVE_SERVICE_STATEDIR] = { .field = "statedir" },
    [E_RESOLVE_SERVICE_EVENTDIR] = { .field = "eventdir" },
    [E_RESOLVE_SERVICE_NOTIFDIR] = { .field = "notifdir" },
    [E_RESOLVE_SERVICE_SUPERVISEDIR] = { .field = "supervisedir" },
    [E_RESOLVE_SERVICE_FDHOLDERDIR] = { .field = "fdholderdir" },
    [E_RESOLVE_SERVICE_ONESHOTDDIR] = { .field = "oneshotddir" },

    // logger
    [E_RESOLVE_SERVICE_LOGNAME] = { .field = "logname" },
    [E_RESOLVE_SERVICE_LOGDESTINATION] = { .field = "logdestination" },
    [E_RESOLVE_SERVICE_LOGBACKUP] = { .field = "logbackup" },
    [E_RESOLVE_SERVICE_LOGMAXSIZE] = { .field = "logmaxsize" },
    [E_RESOLVE_SERVICE_LOGTIMESTAMP] = { .field = "logtimestamp" },
    [E_RESOLVE_SERVICE_LOGWANT] = { .field = "logwant" },
    [E_RESOLVE_SERVICE_LOGRUN] = { .field = "logrun" },
    [E_RESOLVE_SERVICE_LOGRUN_USER] = { .field = "logrun_user" },
    [E_RESOLVE_SERVICE_LOGRUN_BUILD] = { .field = "logrun_build" },
    [E_RESOLVE_SERVICE_LOGRUN_SHEBANG] = { .field = "logrun_shebang" },
    [E_RESOLVE_SERVICE_LOGRUN_RUNAS] = { .field = "logrun_runas" },
    [E_RESOLVE_SERVICE_LOGTIMEOUTSTART] = { .field = "logtimeoutstart" },
    [E_RESOLVE_SERVICE_LOGTIMEOUTSTOP] = { .field = "logtimeoutstop" },

    // environment
    [E_RESOLVE_SERVICE_ENV] = { .field = "env" },
    [E_RESOLVE_SERVICE_ENVDIR] = { .field = "envdir" },
    [E_RESOLVE_SERVICE_ENV_OVERWRITE] = { .field = "env_overwrite" },

    // regex
    [E_RESOLVE_SERVICE_REGEX_CONFIGURE] = { .field = "configure" },
    [E_RESOLVE_SERVICE_REGEX_DIRECTORIES] = { .field = "directories" },
    [E_RESOLVE_SERVICE_REGEX_FILES] = { .field = "files" },
    [E_RESOLVE_SERVICE_REGEX_INFILES] = { .field = "infiles" },
    [E_RESOLVE_SERVICE_REGEX_NDIRECTORIES] = { .field = "ndirectories" },
    [E_RESOLVE_SERVICE_REGEX_NFILES] = { .field = "nfiles" },
    [E_RESOLVE_SERVICE_REGEX_NINFILES] = { .field = "ninfiles" },
    [E_RESOLVE_SERVICE_ENDOFKEY] = { .field = 0 }
} ;
uint32_t resolve_add_uint(char const *data)
{
    uint32_t u ;

    if (!data)
        data = "0" ;
    if (!uint0_scan(data, &u))
        return 0 ;
    return u ;
}

int service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_t field, char const *data)
{
    log_flow() ;

    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(field) {

        // configuration

        case E_RESOLVE_SERVICE_NAME:
            res->name = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_DESCRIPTION:
            res->description = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_VERSION:
            res->version = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_TYPE:
            res->type = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_NOTIFY:
            res->notify = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_MAXDEATH:
            res->maxdeath = resolve_add_uint(data) ; ;
            break ;

        case E_RESOLVE_SERVICE_EARLIER:
            res->earlier = resolve_add_uint(data) ; ;
            break ;

        case E_RESOLVE_SERVICE_HIERCOPY:
            res->hiercopy = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_INTREE:
            res->intree = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_OWNERSTR:
            res->ownerstr = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_OWNER:
            res->owner = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_TREENAME:
            res->treename = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_USER:
            res->user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_INNS:
            res->inns = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENABLED:
            res->enabled = resolve_add_uint(data) ;
            break ;


        // path

        case E_RESOLVE_SERVICE_HOME:
            res->path.home = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FRONTEND:
            res->path.frontend = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_SERVICEDIR:
            res->path.servicedir = resolve_add_string(wres, data) ;
            break ;

        // dependencies

        case E_RESOLVE_SERVICE_DEPENDS:
            res->dependencies.depends = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_REQUIREDBY:
            res->dependencies.requiredby = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_OPTSDEPS:
            res->dependencies.optsdeps = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_CONTENTS:
            res->dependencies.contents = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_NDEPENDS:
            res->dependencies.ndepends = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_NREQUIREDBY:
            res->dependencies.nrequiredby = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_NOPTSDEPS:
            res->dependencies.noptsdeps = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_NCONTENTS:
            res->dependencies.ncontents = resolve_add_uint(data) ;
            break ;

        // execute

        case E_RESOLVE_SERVICE_RUN:
            res->execute.run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_RUN_USER:
            res->execute.run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_RUN_BUILD:
            res->execute.run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_RUN_SHEBANG:
            res->execute.run.shebang = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_RUN_RUNAS:
            res->execute.run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FINISH:
            res->execute.finish.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_USER:
            res->execute.finish.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_BUILD:
            res->execute.finish.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_SHEBANG:
            res->execute.finish.shebang = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FINISH_RUNAS:
            res->execute.finish.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTSTART:
            res->execute.timeout.start = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_TIMEOUTSTOP:
            res->execute.timeout.stop = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DOWN:
            res->execute.down = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_DOWNSIGNAL:
            res->execute.downsignal = resolve_add_uint(data) ;
            break ;

        // live

        case E_RESOLVE_SERVICE_LIVEDIR:
            res->live.livedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_STATUS:
            res->live.status = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_SERVICEDIR_LIVE:
            res->live.servicedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_SCANDIR:
            res->live.scandir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_STATEDIR:
            res->live.statedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_EVENTDIR:
            res->live.eventdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_NOTIFDIR:
            res->live.notifdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_SUPERVISEDIR:
            res->live.supervisedir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_FDHOLDERDIR:
            res->live.fdholderdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ONESHOTDDIR:
            res->live.oneshotddir = resolve_add_string(wres, data) ;
            break ;

        // logger

        case E_RESOLVE_SERVICE_LOGNAME:
            res->logger.name = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGDESTINATION:
            res->logger.destination = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGBACKUP:
            res->logger.backup = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGMAXSIZE:
            res->logger.maxsize = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMESTAMP:
            res->logger.timestamp = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGWANT:
            res->logger.want = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN:
            res->logger.execute.run.run = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_USER:
            res->logger.execute.run.run_user = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_BUILD:
            res->logger.execute.run.build = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_SHEBANG:
            res->logger.execute.run.shebang = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGRUN_RUNAS:
            res->logger.execute.run.runas = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMEOUTSTART:
            res->logger.timeout.start = resolve_add_uint(data) ;
            break ;

        case E_RESOLVE_SERVICE_LOGTIMEOUTSTOP:
            res->logger.timeout.stop = resolve_add_uint(data) ;
            break ;

        // environment

        case E_RESOLVE_SERVICE_ENV:
            res->environ.env = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENVDIR:
            res->environ.envdir = resolve_add_string(wres, data) ;
            break ;

        case E_RESOLVE_SERVICE_ENV_OVERWRITE:
            res->environ.env_overwrite = resolve_add_uint(data) ;
            break ;

        // regex

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
            return e ;
    }

    e = 1 ;

   free(wres) ;
   return e ;

}
