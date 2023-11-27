/*
 * write_logger.c
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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>

#include <skalibs/types.h>

#include <66/config.h>
#include <66/write.h>
#include <66/enum.h>
#include <66/utils.h>
#include <s6/config.h>
#include <66/constants.h>
#include <66/parse.h>

#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

/** @destination -> /var/lib/66/system/service/svc/<name> */

void write_logger(resolve_service_t *res, char const *destination, uint8_t force)
{
    log_flow() ;

    uid_t log_uid ;
    gid_t log_gid ;
    uint8_t owner = res->owner ;

    char *logrunner = res->execute.run.runas ? res->sa.s + res->execute.run.runas : SS_LOGGER_RUNNER ;

    if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid)) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;
    }

    if (res->execute.timeout.kill) {
        if (!write_uint(destination, "timeout-kill", res->execute.timeout.kill)) {
            parse_cleanup(res, destination, force) ;
            log_dieusys(LOG_EXIT_SYS, "write uint file timeout-kill") ;
        }
    }

    if (res->execute.timeout.finish) {
        if (!write_uint(destination, "timeout-finish", res->execute.timeout.finish)) {
            parse_cleanup(res, destination, force) ;
            log_dieusys(LOG_EXIT_SYS, "write uint file timeout-finish") ;
        }
    }

    /** notification */
    if (!write_uint(destination, SS_NOTIFICATION, 3)) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "write uint file ", SS_NOTIFICATION) ;
    }

    /** log destination
     *
     * The creation of the log destination directory is made at sanitize_init
     * to avoid none existing directory if the destination is on tmpfs directory
     */

    char write[strlen(destination) + 10] ;

    /** run script */
    log_trace("write file: ", destination, "/run") ;
    if (!file_write_unsafe(destination, "run", res->sa.s + res->execute.run.run, strlen(res->sa.s + res->execute.run.run))) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "write: ", destination, "/run.user") ;
    }

    auto_strings(write, destination, "/run") ;

    if (chmod(write, 0755) < 0) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
    }

    /** run.user script */
    log_trace("write file: ", destination, "/run.user") ;
    if (!file_write_unsafe(destination, "run.user", res->sa.s + res->execute.run.run_user, strlen(res->sa.s + res->execute.run.run_user))) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "write: ", destination, "/run.user") ;
    }

    auto_strings(write, destination, "/run.user") ;

    if (chmod(write, 0755) < 0) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
    }

    if (!owner) {

        if (chown(write, log_uid, log_gid) == -1) {
            parse_cleanup(res, destination, force) ;
            log_dieusys(LOG_EXIT_SYS, "chown: ", write) ;
        }
    }
}
