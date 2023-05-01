/*
 * sanitize_write.c
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

#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/sanitize.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/state.h>

static int delete(resolve_service_t *res, uint8_t force)
{
    int r ;

    char dir[strlen(res->sa.s + res->path.servicedir) + 1] ;

    auto_strings(dir, res->sa.s + res->path.servicedir) ;

    r = scan_mode(dir, S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "unvalid source: ", dir) ;

    if (r && force) {

        log_trace("remove directory: ", dir) ;

        if (!dir_rm_rf(dir))
            log_dieusys(LOG_EXIT_SYS, "remove: ", dir) ;

    } else if (r && !force) {

        log_info("Ignoring: ", res->sa.s + res->name, " service: already written") ;
        return 2 ;
    }

    return 1 ;
}

static void resolve_compare(resolve_service_t *res)
{
    resolve_service_t fres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &fres) ;
    ss_state_t ste = STATE_ZERO ;
    char *name = res->sa.s + res->name ;

    if (resolve_check_g(wres, res->sa.s + res->path.home, name)) {

        if (!resolve_read_g(wres, res->sa.s + res->path.home, name))
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;

        if (state_check(&fres)) {

            if (!state_read(&ste, &fres))
                log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

            if (fres.type != res->type && FLAGS_ISSET(ste.issupervised, STATE_FLAGS_TRUE))
                log_die(LOG_EXIT_SYS, "Detection of incompatible type format for supervised service: ", name, " -- current: ", get_key_by_enum(ENUM_TYPE, res->type), " previous: ", get_key_by_enum(ENUM_TYPE, fres.type), ". Please unsupervise it with '66 unsupervice ", name,"' before trying the conversion") ;
        }

        if (strcmp(res->sa.s + res->treename, fres.sa.s + fres.treename))
            tree_service_remove(fres.sa.s + fres.path.home, fres.sa.s + fres.treename, name) ;
    }

    resolve_free(wres) ;
}

void sanitize_write(resolve_service_t *res, uint8_t force)
{
    /** create symlink to .resolve/ directory */
    int r ;
    size_t homelen = strlen(res->sa.s + res->path.servicedir) ;

    resolve_compare(res) ;

    if (delete(res, force) > 1)
        return ;

    char sym[homelen + SS_RESOLVE_LEN + 1] ;

    auto_strings(sym, res->sa.s + res->path.servicedir) ;

    r = scan_mode(sym, S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "unvalid source: ", sym) ;

    log_trace("create directory: ", sym) ;
    if (!dir_create_parent(sym, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", sym) ;

    char dst[SS_RESOLVE_LEN + 5 + 1] ;

    auto_strings(dst, "../.." SS_RESOLVE) ;
    auto_strings(sym + homelen, SS_RESOLVE) ;

    log_trace("symlink: ", sym, " to: ", dst) ;
    r = symlink(dst, sym) ;
    if (r < 0 && errno != EEXIST)
        log_dieusys(LOG_EXIT_SYS, "point symlink: ", sym, " to: ", dst) ;


}

