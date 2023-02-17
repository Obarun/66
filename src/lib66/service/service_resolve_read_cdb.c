/*
 * service_resolve_read_cdb.c
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
#include <stdlib.h>//free

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>

#include <66/resolve.h>
#include <66/service.h>

int service_resolve_read_cdb(cdb *c, resolve_service_t *res)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    /* configuration */
    resolve_find_cdb(&tmp,c,"name") ;
    res->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"description") ;
    res->description = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"version") ;
    res->version = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"type") ;
    res->type = x ;
    x = resolve_find_cdb(&tmp,c,"notify") ;
    res->notify = x ;
    x = resolve_find_cdb(&tmp,c,"maxdeath") ;
    res->maxdeath = x ;
    x = resolve_find_cdb(&tmp,c,"earlier") ;
    res->earlier = x ;
    resolve_find_cdb(&tmp,c,"hiercopy") ;
    res->hiercopy = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"intree") ;
    res->intree = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"ownerstr") ;
    res->ownerstr = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"owner") ;
    res->owner = x ;
    resolve_find_cdb(&tmp,c,"treename") ;
    res->treename = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"user") ;
    res->user = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"inmodule") ;
    res->inmodule = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* path configuration */
    resolve_find_cdb(&tmp,c,"home") ;
    res->path.home = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"frontend") ;
    res->path.frontend = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"status") ;
    res->path.status = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* dependencies */
    resolve_find_cdb(&tmp,c,"depends") ;
    res->dependencies.depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"requiredby") ;
    res->dependencies.requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"optsdeps") ;
    res->dependencies.optsdeps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"ndepends") ;
    res->dependencies.ndepends = x ;
    x = resolve_find_cdb(&tmp,c,"nrequiredby") ;
    res->dependencies.nrequiredby = x ;
    x = resolve_find_cdb(&tmp,c,"noptsdeps") ;
    res->dependencies.noptsdeps = x ;

    /* execute */
    resolve_find_cdb(&tmp,c,"run") ;
    res->execute.run.run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"run_user") ;
    res->execute.run.run_user = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"run_build") ;
    res->execute.run.build = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"run_shebang") ;
    res->execute.run.shebang = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"run_runas") ;
    res->execute.run.runas = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"finish") ;
    res->execute.finish.run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"finish_user") ;
    res->execute.finish.run_user = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"finish_build") ;
    res->execute.finish.build = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"finish_shebang") ;
    res->execute.finish.shebang = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"finish_runas") ;
    res->execute.finish.runas = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"timeoutkill") ;
    res->execute.timeout.kill = x ;
    x = resolve_find_cdb(&tmp,c,"timeoutfinish") ;
    res->execute.timeout.finish = x ;
    x = resolve_find_cdb(&tmp,c,"timeoutup") ;
    res->execute.timeout.up = x ;
    x = resolve_find_cdb(&tmp,c,"timeoutdown") ;
    res->execute.timeout.down = x ;
    x = resolve_find_cdb(&tmp,c,"down") ;
    res->execute.down = x ;
    x = resolve_find_cdb(&tmp,c,"downsignal") ;
    res->execute.downsignal = x ;

    /* live */
    resolve_find_cdb(&tmp,c,"livedir") ;
    res->live.livedir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"scandir") ;
    res->live.scandir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"statedir") ;
    res->live.statedir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"eventdir") ;
    res->live.eventdir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"notifdir") ;
    res->live.notifdir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"supervisedir") ;
    res->live.supervisedir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"fdholderdir") ;
    res->live.fdholderdir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"oneshotddir") ;
    res->live.oneshotddir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logger */
    resolve_find_cdb(&tmp,c,"logname") ;
    res->logger.name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"logdestination") ;
    res->logger.destination = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"logbackup") ;
    res->logger.backup = x ;
    x = resolve_find_cdb(&tmp,c,"logmaxsize") ;
    res->logger.maxsize = x ;
    x = resolve_find_cdb(&tmp,c,"logtimestamp") ;
    res->logger.timestamp = x ;
    x = resolve_find_cdb(&tmp,c,"logwant") ;
    res->logger.want = x ;
    resolve_find_cdb(&tmp,c,"logrun") ;
    res->logger.execute.run.run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"logrun_user") ;
    res->logger.execute.run.run_user = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"logrun_build") ;
    res->logger.execute.run.build = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"logrun_shebang") ;
    res->logger.execute.run.shebang = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"logrun_runas") ;
    res->logger.execute.run.runas = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"logtimeoutkill") ;
    res->logger.timeout.kill = x ;
    x = resolve_find_cdb(&tmp,c,"logtimeoutfinish") ;
    res->logger.timeout.finish = x ;

    /* environment */
    resolve_find_cdb(&tmp,c,"env") ;
    res->environ.env = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"envdir") ;
    res->environ.envdir = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"env_overwrite") ;
    res->environ.env_overwrite = x ;

    /* regex */
    resolve_find_cdb(&tmp,c,"configure") ;
    res->regex.configure = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"directories") ;
    res->regex.directories = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"files") ;
    res->regex.files = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    resolve_find_cdb(&tmp,c,"infiles") ;
    res->regex.infiles = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;
    x = resolve_find_cdb(&tmp,c,"ndirectories") ;
    res->regex.ndirectories = x ;
    x = resolve_find_cdb(&tmp,c,"nfiles") ;
    res->regex.nfiles = x ;
    x = resolve_find_cdb(&tmp,c,"ninfiles") ;
    res->regex.ninfiles = x ;


    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}
