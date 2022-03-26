/*
 * service_resolve_setnwrite.c
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

int service_resolve_setnwrite(sv_alltype *services, ssexec_t *info, char const *dst)
{
    log_flow() ;

    int e = 0 ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner), id, nid ;
    ownerstr[ownerlen] = 0 ;

    stralloc destlog = STRALLOC_ZERO ;
    stralloc ndeps = STRALLOC_ZERO ;
    stralloc other_deps = STRALLOC_ZERO ;

    ss_state_t sta = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    char *name = keep.s + services->cname.name ;
    size_t namelen = strlen(name) ;
    char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
    char logreal[namelen + SS_LOG_SUFFIX_LEN + 1] ;
    char stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + namelen + SS_LOG_SUFFIX_LEN + 1] ;

    size_t livelen = info->live.len - 1 ;
    char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1 + namelen + 1] ;
    auto_strings(state, info->live.s, SS_STATE, "/", ownerstr, "/", info->treename.s) ;


    res.type = services->cname.itype ;
    res.name = resolve_add_string(wres,name) ;
    res.description = resolve_add_string(wres,keep.s + services->cname.description) ;
    res.version = resolve_add_string(wres,keep.s + services->cname.version) ;
    res.tree = resolve_add_string(wres,info->tree.s) ;
    res.treename = resolve_add_string(wres,info->treename.s) ;
    res.live = resolve_add_string(wres,info->live.s) ;
    res.state = resolve_add_string(wres,state) ;
    res.src = resolve_add_string(wres,keep.s + services->src) ;

    if (services->srconf > 0)
        res.srconf = resolve_add_string(wres,keep.s + services->srconf) ;

    if (res.type == TYPE_ONESHOT) {

        if (services->type.oneshot.up.exec >= 0) {

            res.exec_run = resolve_add_string(wres,keep.s + services->type.oneshot.up.exec) ;
            res.real_exec_run = resolve_add_string(wres,keep.s + services->type.oneshot.up.real_exec) ;
        }

        if (services->type.oneshot.down.exec >= 0) {

            res.exec_finish = resolve_add_string(wres,keep.s + services->type.oneshot.down.exec) ;
            res.real_exec_finish = resolve_add_string(wres,keep.s + services->type.oneshot.down.real_exec) ;
        }
    }

    if (res.type == TYPE_CLASSIC || res.type == TYPE_LONGRUN) {

        if (services->type.classic_longrun.run.exec >= 0) {
            res.exec_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.run.exec) ;
            res.real_exec_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.run.real_exec) ;
        }

        if (services->type.classic_longrun.finish.exec >= 0) {

            res.exec_finish = resolve_add_string(wres,keep.s + services->type.classic_longrun.finish.exec) ;
            res.real_exec_finish = resolve_add_string(wres,keep.s + services->type.classic_longrun.finish.real_exec) ;
        }
    }

    res.ndepends = services->cname.nga ;
    res.noptsdeps = services->cname.nopts ;
    res.nextdeps = services->cname.next ;
    res.ncontents = services->cname.ncontents ;

    if (services->flags[0])
        res.down = 1 ;

    res.disen = 1 ;

    if (res.type == TYPE_CLASSIC) {

        auto_strings(stmp, info->scandir.s, "/", name) ;
        res.runat = resolve_add_string(wres,stmp) ;

    } else if (res.type >= TYPE_BUNDLE) {

        auto_strings(stmp, info->livetree.s, "/", info->treename.s, SS_SVDIRS, "/", name) ;
        res.runat = resolve_add_string(wres,stmp) ;
    }

    if (state_check(state,name)) {

        if (!state_read(&sta,state,name)) {
            log_warnusys("read state file of: ",name) ;
            goto err ;
        }

        if (!sta.init)
            state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;

        if (sta.init) {

            state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;

        } else {

            state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
        }

        state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;

        if (!state_write(&sta,res.sa.s + res.state,name)) {
            log_warnusys("write state file of: ",name) ;
            goto err ;
        }
    }

    if (res.ndepends)
    {
        id = services->cname.idga, nid = res.ndepends ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&ndeps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&ndeps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        ndeps.len-- ;

        if (!stralloc_0(&ndeps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.depends = resolve_add_string(wres,ndeps.s) ;
    }

    if (res.noptsdeps)
    {
        id = services->cname.idopts, nid = res.noptsdeps ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;

        if (!stralloc_0(&other_deps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.optsdeps = resolve_add_string(wres,other_deps.s) ;
    }

    if (res.nextdeps)
    {
        other_deps.len = 0 ;
        id = services->cname.idext, nid = res.nextdeps ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)){
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;

        if (!stralloc_0(&other_deps)){
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.extdeps = resolve_add_string(wres,other_deps.s) ;
    }

    if (res.ncontents) {

        other_deps.len = 0 ;
        id = services->cname.idcontents, nid = res.ncontents ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;
        if (!stralloc_0(&other_deps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }
        res.contents = resolve_add_string(wres,other_deps.s) ;
    }

    if (services->opts[0]) {

        // destination of the logger
        if (services->type.classic_longrun.log.destination < 0) {

            if(info->owner > 0) {

                if (!auto_stra(&destlog, get_userhome(info->owner), "/", SS_LOGGER_USERDIR, name)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }

            } else {

                if (!auto_stra(&destlog, SS_LOGGER_SYSDIR, name)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
            }

        } else {

            if (!auto_stra(&destlog,keep.s + services->type.classic_longrun.log.destination)) {
                log_warnsys("stralloc") ;
                goto err ;
            }
        }

        res.dstlog = resolve_add_string(wres,destlog.s) ;

        if ((res.type == TYPE_CLASSIC) || (res.type == TYPE_LONGRUN)) {

            auto_strings(logname, name, SS_LOG_SUFFIX) ;
            auto_strings(logreal, name) ;

            if (res.type == TYPE_CLASSIC) {
                auto_strings(logreal + namelen, "/log") ;

            } else {

                auto_strings(logreal + namelen,"-log") ;
            }

            res.logger = resolve_add_string(wres,logname) ;
            res.logreal = resolve_add_string(wres,logreal) ;
            if (ndeps.len)
                ndeps.len--;

            if (!stralloc_catb(&ndeps," ",1) ||
                !stralloc_cats(&ndeps,res.sa.s + res.logger) ||
                !stralloc_0(&ndeps)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }

            res.depends = resolve_add_string(wres,ndeps.s) ;

            if (res.type == TYPE_CLASSIC) {

                res.ndepends = 1 ;

            } else if (res.type == TYPE_LONGRUN) {

                res.ndepends += 1 ;
            }

            if (services->type.classic_longrun.log.run.exec >= 0)
                res.exec_log_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.log.run.exec) ;

            if (services->type.classic_longrun.log.run.real_exec >= 0)
                res.real_exec_log_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.log.run.real_exec) ;

            if (!service_resolve_setlognwrite(&res,dst))
                goto err ;
        }
    }

    if (!resolve_write(wres,dst,res.sa.s + res.name)) {

        log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
        goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&ndeps) ;
        stralloc_free(&other_deps) ;
        stralloc_free(&destlog) ;
        return e ;
}
