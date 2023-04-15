/*
 * service_resolve_copy.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_copy(resolve_service_t *dst, resolve_service_t *res)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = res->sa.len - 1 ;
    dst->salen = res->salen ;

    if (!stralloc_catb(&dst->sa,res->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    // configuration
    dst->name = res->name ;
    dst->description = res->description ;
    dst->version = res->version ;
    dst->type = res->type ;
    dst->notify = res->notify ;
    dst->maxdeath = res->maxdeath ;
    dst->earlier = res->earlier ;
    dst->hiercopy = res->hiercopy ;
    dst->intree = res->intree ;
    dst->ownerstr = res->ownerstr ;
    dst->owner = res->owner ;
    dst->treename = res->treename ;
    dst->user = res->user ;
    dst->inmodule = res->inmodule ;

    // path
    dst->path.home = res->path.home ;
    dst->path.frontend = res->path.frontend ;
    dst->path.status = res->path.status ;
    dst->path.servicedir = res->path.servicedir ;

    // dependencies
    dst->dependencies.depends = res->dependencies.depends ;
    dst->dependencies.requiredby = res->dependencies.requiredby ;
    dst->dependencies.optsdeps = res->dependencies.optsdeps ;
    dst->dependencies.contents = res->dependencies.contents ;
    dst->dependencies.ndepends = res->dependencies.ndepends ;
    dst->dependencies.nrequiredby = res->dependencies.nrequiredby ;
    dst->dependencies.noptsdeps = res->dependencies.noptsdeps ;
    dst->dependencies.ncontents = res->dependencies.ncontents ;

    // execute
    dst->execute.run.run = res->execute.run.run ;
    dst->execute.run.run_user = res->execute.run.run_user ;
    dst->execute.run.build = res->execute.run.build ;
    dst->execute.run.shebang = res->execute.run.shebang ;
    dst->execute.run.runas = res->execute.run.runas ;
    dst->execute.finish.run = res->execute.finish.run ;
    dst->execute.finish.run_user = res->execute.finish.run_user ;
    dst->execute.finish.build = res->execute.finish.build ;
    dst->execute.finish.shebang = res->execute.finish.shebang ;
    dst->execute.finish.runas = res->execute.finish.runas ;
    dst->execute.timeout.kill = res->execute.timeout.kill ;
    dst->execute.timeout.finish = res->execute.timeout.finish ;
    dst->execute.timeout.up = res->execute.timeout.up ;
    dst->execute.timeout.down = res->execute.timeout.down ;
    dst->execute.down = res->execute.down ;
    dst->execute.downsignal = res->execute.downsignal ;

    // live
    dst->live.livedir = res->live.livedir ;
    dst->live.scandir = res->live.scandir ;
    dst->live.statedir = res->live.statedir ;
    dst->live.eventdir = res->live.eventdir ;
    dst->live.notifdir = res->live.notifdir ;
    dst->live.supervisedir = res->live.supervisedir ;
    dst->live.fdholderdir = res->live.fdholderdir ;
    dst->live.oneshotddir = res->live.oneshotddir ;

    // logger
    dst->logger.name = res->logger.name ;
    dst->logger.destination = res->logger.destination ;
    dst->logger.backup = res->logger.backup ;
    dst->logger.maxsize = res->logger.maxsize ;
    dst->logger.timestamp = res->logger.timestamp ;
    dst->logger.want = res->logger.want ;
    dst->logger.execute.run.run = res->logger.execute.run.run ;
    dst->logger.execute.run.run_user = res->logger.execute.run.run_user ;
    dst->logger.execute.run.build = res->logger.execute.run.build ;
    dst->logger.execute.run.shebang = res->logger.execute.run.shebang ;
    dst->logger.execute.run.runas = res->logger.execute.run.runas ;
    dst->logger.timeout.kill = res->logger.timeout.kill ;
    dst->logger.timeout.finish = res->logger.timeout.finish ;

    // environment
    dst->environ.env = res->environ.env ;
    dst->environ.envdir = res->environ.envdir ;
    dst->environ.env_overwrite = res->environ.env_overwrite ;

    // regex
    dst->regex.configure = res->regex.configure ;
    dst->regex.directories = res->regex.directories ;
    dst->regex.files = res->regex.files ;
    dst->regex.infiles = res->regex.infiles ;
    dst->regex.ndirectories = res->regex.ndirectories ;
    dst->regex.nfiles = res->regex.nfiles ;
    dst->regex.ninfiles = res->regex.ninfiles ;

    return 1 ;
}
