/*
 * sanitize_scandir.c
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

#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <66/service.h>
#include <66/sanitize.h>
#include <66/constants.h>
#include <66/enum_parser.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/symlink.h>

static void scandir_to_livestate(resolve_service_t *res)
{
    log_flow() ;

    char const *sym = res->sa.s + res->live.scandir ;

    log_trace("symlink: ", sym, " to: ", res->sa.s + res->live.servicedir) ;
    if (!symlink_atomic(res->sa.s + res->live.servicedir, sym))
       log_dieu(LOG_EXIT_SYS, "symlink: ", sym, " to: ", res->sa.s + res->live.servicedir) ;
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

        if (res->type == E_PARSER_TYPE_CLASSIC)
            scandir_to_livestate(res) ;

        state_set_flag(sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;
        state_set_flag(sta, STATE_FLAGS_TOUNSUPERVISE, STATE_FLAGS_FALSE) ;

    } else {

        if (sta->toinit == STATE_FLAGS_TRUE) {

            file_tryunlink(res->sa.s + res->live.scandir) ;
            scandir_to_livestate(res) ;
        }

        if (sta->tounsupervise == STATE_FLAGS_TRUE) {

            log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
            file_tryunlink(res->sa.s + res->live.scandir) ;

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
