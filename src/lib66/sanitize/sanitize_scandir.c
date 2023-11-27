/*
 * sanitize_scandir.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h> // umask

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/unix-transactional.h>
#include <skalibs/posixplz.h>

#include <66/service.h>
#include <66/sanitize.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/state.h>
#include <66/svc.h>

static void scandir_scandir_to_livestate(resolve_service_t *res)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;

    char sym[livelen + SS_SCANDIR_LEN + 1 + ownerlen + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->live.livedir, SS_SCANDIR, "/", res->sa.s + res->ownerstr, "/", name) ;

    log_trace("symlink: ", sym, " to: ", res->sa.s + res->live.servicedir) ;
    if (!atomic_symlink(res->sa.s + res->live.servicedir, sym, "scandir"))
       log_dieu(LOG_EXIT_SYS, "symlink: ", sym, " to: ", res->sa.s + res->live.servicedir) ;
}

int scandir_supervision_dir(resolve_service_t *res)
{
    log_flow() ;

    mode_t hmod = umask(0) ;
    char *event = res->sa.s + res->live.eventdir ;
    char *supervise = res->sa.s + res->live.supervisedir ;

    /* event dir */
    log_trace("create directory: ", event) ;
    int r = dir_create_parent(event, 0700) ;
    if (!r)
        log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", event) ;

    if (chown(event, -1, getegid()) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chown: ", event) ;

    if (chmod(event, 03730) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chmod: ", event) ;

    /* supervise dir */
    log_trace("create directory: ", supervise) ;
    r = dir_create_parent(supervise, 0700) ;
    if (!r)
        log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", event) ;

    umask(hmod) ;

    return 1 ;
}

int sanitize_scandir(resolve_service_t *res, ss_state_t *sta)
{
    log_flow() ;

    int r ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t scandirlen = livelen + SS_SCANDIR_LEN + 1 + strlen(res->sa.s + res->ownerstr)  ;
    char svcandir[scandirlen + 1] ;

    auto_strings(svcandir, res->sa.s + res->live.livedir, SS_SCANDIR, "/", res->sa.s + res->ownerstr) ;

    r = access(res->sa.s + res->live.scandir, F_OK) ;
    if (r == -1 && (sta->toinit == STATE_FLAGS_TRUE || res->earlier)) {

        if (res->type == TYPE_CLASSIC)
            scandir_scandir_to_livestate(res) ;

        state_set_flag(sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;
        state_set_flag(sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE) ;

    } else {

        if (sta->tounsupervise == STATE_FLAGS_TRUE) {

            log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
            unlink_void(res->sa.s + res->live.scandir) ;

            state_set_flag(sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_FALSE) ;
            state_set_flag(sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE) ;

            if (svc_scandir_send(svcandir, "an") <= 0)
                log_warnu_return(LOG_EXIT_ZERO, "reload scandir: ", svcandir) ;

        } else if (sta->toreload == STATE_FLAGS_TRUE) {

            if (svc_scandir_send(svcandir, "a") <= 0)
                log_warnu_return(LOG_EXIT_ZERO, "reload scandir: ", svcandir) ;

            state_set_flag(sta, STATE_FLAGS_TORELOAD, STATE_FLAGS_FALSE) ;
            state_set_flag(sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE) ;
            state_set_flag(sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;
        }
    }
    return 1 ;
}
