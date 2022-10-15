/*
 * sanitize_livestate.c
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
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/unix-transactional.h>
#include <skalibs/posixplz.h>

#include <66/constants.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/utils.h>
#include <66/state.h>

/** creation of the /run/66/state/<uid> directory */

static void sanitize_livestate_directory(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    gid_t gidowner ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;
    char ste[livelen + SS_STATE_LEN + 1 + ownerlen + 1] ;

    auto_strings(ste, res->sa.s + res->live.livedir, SS_STATE + 1, "/", res->sa.s + res->ownerstr) ;

    r = scan_mode(ste, S_IFDIR) ;
    if (r < 0)
        log_diesys(LOG_EXIT_SYS, "conflicting format for: ", ste) ;
    if (!r) {

        r = dir_create_parent(ste, 0700) ;
        if (!r)
            log_dieusys(LOG_EXIT_SYS, "create directory: ", ste) ;

        if (!yourgid(&gidowner, res->owner))
            log_dieusys(LOG_EXIT_SYS, "get gid of: ", res->sa.s + res->ownerstr) ;

        if (chown(ste, res->owner, gidowner) < 0)
            log_dieusys(LOG_EXIT_SYS, "chown: ", ste) ;
    }
}

/** creation of the /run/66/state/<uid>/<service> symlink */

static void sanitize_livestate_service_symlink(resolve_service_t *res)
{
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->ownerstr) ;
    size_t treelen = strlen(res->sa.s + res->path.tree) ;

    char sym[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + namelen + 1] ;
    char dst[treelen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

    auto_strings(sym, res->sa.s + res->live.livedir, SS_STATE + 1, "/", res->sa.s + res->ownerstr, "/", name) ;

    auto_strings(dst, res->sa.s + res->path.tree, SS_SVDIRS, SS_SVC, "/", name) ;

    if (!atomic_symlink(dst, sym, "scandir"))
       log_dieu(LOG_EXIT_SYS, "symlink: ", sym, " to: ", dst) ;
}

void sanitize_livestate(resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    int r ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    size_t livelen = strlen(res->sa.s + res->live.livedir) ;
    size_t ownerlen = strlen(res->sa.s + res->owner) ;

    char ste[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + namelen + 1] ;
    auto_strings(ste, res->sa.s + res->live.livedir, SS_STATE + 1, "/", res->sa.s + res->ownerstr, "/", name) ;

    r = access(ste, F_OK) ;
    if (r == -1) {

        sanitize_livestate_directory(res) ;

        sanitize_livestate_service_symlink(res) ;

    } else {

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE)) {

            unlink_void(ste) ;

            if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TORELOAD, STATE_FLAGS_FALSE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;
        }
    }

    if (!state_messenger(res->sa.s + res->path.home, name, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE))
        log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

}
