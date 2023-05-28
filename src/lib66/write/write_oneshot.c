/*
 * write_oneshot.c
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

#include <oblibs/log.h>

#include <66/service.h>
#include <66/write.h>
#include <66/parse.h>

void write_oneshot(resolve_service_t *res, char const *dst, uint8_t force)
{
    log_flow() ;

    /**notification,timeout, ... */
    if (!write_common(res, dst)) {
        parse_cleanup(res, dst, force) ;
        log_dieu(LOG_EXIT_SYS, "write common file of: ", res->sa.s + res->name) ;
    }

    /** run file */
    if (!write_execute_scripts("up", res->sa.s + res->execute.run.run, dst)) {
        parse_cleanup(res, dst, force) ;
        log_dieu(LOG_EXIT_SYS, "write execute script of: ", res->sa.s + res->name) ;
    }

    /** finish file */
    if (res->execute.finish.run_user) {
        if (!write_execute_scripts("down", res->sa.s + res->execute.finish.run, dst)) {
            parse_cleanup(res, dst, force) ;
            log_dieu(LOG_EXIT_SYS, "write execute script of: ", res->sa.s + res->name) ;
        }
    }

    /** run.user file */
    if (!write_execute_scripts("up.user", res->sa.s + res->execute.run.run_user, dst)) {
        parse_cleanup(res, dst, force) ;
        log_dieu(LOG_EXIT_SYS, "write execute script of: ", res->sa.s + res->name) ;
    }

    /** finish.user file */
    if (res->execute.finish.run_user) {
        if (!write_execute_scripts("down.user", res->sa.s + res->execute.finish.run_user, dst)) {
            parse_cleanup(res, dst, force) ;
            log_dieu(LOG_EXIT_SYS, "write execute script of: ", res->sa.s + res->name) ;
        }
    }
}
