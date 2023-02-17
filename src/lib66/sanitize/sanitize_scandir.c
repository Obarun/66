/*
 * sanitize_scandir.c
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
#include <unistd.h>
#include <stdint.h>
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
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;

    char sym[livelen + 1 + SS_SCANDIR_LEN + 1 + ownerlen + 1 + namelen + 1] ;
    char dst[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->live.livedir, "/", SS_SCANDIR, "/", res->sa.s + res->ownerstr, "/", name) ;

    auto_strings(dst, res->sa.s + res->live.livedir, SS_STATE + 1, "/", res->sa.s + res->ownerstr, "/", name) ;

    if (!atomic_symlink(dst, sym, "scandir"))
       log_dieu(LOG_EXIT_SYS, "symlink: ", sym, " to: ", dst) ;
}

static void scandir_service_to_scandir(resolve_service_t *res)
{
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t homelen = strlen(res->sa.s + res->path.home) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;

    char sym[homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1 + SS_SCANDIR_LEN + 1] ;
    char dst[livelen + SS_SCANDIR_LEN + 1 + ownerlen + 1] ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE
    , SS_SVC, "/", name, "/", SS_SCANDIR) ;

    auto_strings(dst, res->sa.s + res->live.livedir, SS_SCANDIR, "/", res->sa.s + res->ownerstr) ;

    if (!atomic_symlink(dst, sym, "scandir"))
       log_dieu(LOG_EXIT_SYS, "symlink: ", sym, " to: ", dst) ;
}

static void compute_supervision_dir(resolve_service_t *res)
{
    mode_t hmod = umask(0) ;
    char *event = res->sa.s + res->live.eventdir ;
    char *supervise = res->sa.s + res->live.supervisedir ;

    /* event dir */
    int r = dir_create_parent(event, 0700) ;
    if (!r)
        log_dieusys(LOG_EXIT_SYS, "create directory: ", event) ;

    if (chown(event, -1, getegid()) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", event) ;

    if (chmod(event, 03730) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod: ", event) ;

    /* supervise dir*/
    r = dir_create_parent(supervise, 0700) ;
    if (!r)
        log_dieusys(LOG_EXIT_SYS, "create directory: ", event) ;

    umask(hmod) ;
}

void sanitize_scandir(resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t scandirlen = livelen + SS_SCANDIR_LEN + 1 + strlen(res->sa.s + res->ownerstr)  ;

    char svcandir[scandirlen + 1] ;
    auto_strings(svcandir, res->sa.s + res->live.livedir, SS_SCANDIR, "/", res->sa.s + res->ownerstr) ;

    r = access(res->sa.s + res->live.scandir, F_OK) ;
    if (r == -1 && FLAGS_ISSET(flag, STATE_FLAGS_TOINIT)) {

        if (res->type == TYPE_CLASSIC)
            scandir_scandir_to_livestate(res) ;

        scandir_service_to_scandir(res) ;

        compute_supervision_dir(res) ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_ISEARLIER)) {

            if (svc_scandir_send(svcandir, "h") <= 0)
                log_dieu(LOG_EXIT_SYS, "reload scandir: ", svcandir) ;
        }

        if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE))
           log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

        if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

    } else {

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE)) {

            char s[livelen + SS_SCANDIR_LEN + 1 + strlen(res->sa.s + res->ownerstr) + 1 + namelen + 1] ;
            auto_strings(s, res->sa.s + res->live.livedir, SS_SCANDIR, "/", res->sa.s + res->ownerstr, "/", name) ;

            unlink_void(s) ;

            if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_FALSE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

            if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

            if (svc_scandir_send(svcandir, "an") <= 0)
                log_dieu(LOG_EXIT_SYS, "reload scandir: ", svcandir) ;
        }

        if (FLAGS_ISSET(flag, STATE_FLAGS_TORELOAD)) {

            if (svc_scandir_send(svcandir, "a") <= 0)
                log_dieu(LOG_EXIT_SYS, "reload scandir: ", svcandir) ;

            if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TORELOAD, STATE_FLAGS_FALSE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

            if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;
        }
    }
}
