/*
 * write_logger.c
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

#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

/** @destination -> /var/lib/66/system/<tree>/servicedirs/svc/ */

void write_logger(resolve_service_t *res, char const *destination, uint8_t force)
{
    log_flow() ;

    int r ;
    uid_t log_uid ;
    gid_t log_gid ;
    uint8_t owner = res->owner ;

    char *logrunner = res->logger.execute.run.runas ? res->sa.s + res->logger.execute.run.runas : SS_LOGGER_RUNNER ;

    char dst[strlen(destination) + strlen(res->sa.s + res->logger.name) + 1] ;
    auto_strings(dst, destination, res->sa.s + res->logger.name) ;

    r = scan_mode(dst, S_IFDIR) ;
    if (r && force) {

        if (!dir_rm_rf(dst))
            log_dieusys(LOG_EXIT_SYS, "delete: ", dst) ;

    } else if (r) {

        log_warn("ignoring ", dst, " -- already exist") ;
        return ;
    }

    if (!dir_create_parent(dst, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", dst) ;

    if (res->logger.execute.timeout.kill)
        write_uint(dst, "timeout-kill", res->logger.execute.timeout.kill) ;

    if (res->logger.execute.timeout.finish)
        write_uint(dst, "timeout-finish", res->logger.execute.timeout.finish) ;

    /** notification */
    write_uint(dst, "notification-fd", 3) ;

    /** log destination */
    if (!dir_create_parent(res->sa.s + res->logger.destination, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", res->sa.s + res->logger.destination) ;

    if (!owner && ((res->logger.execute.run.build == BUILD_AUTO) || (!res->logger.execute.run.build))) {

        if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid))
            log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;

        if (chown(res->sa.s + res->logger.destination, log_uid, log_gid) == -1)
            log_dieusys(LOG_EXIT_SYS, "chown: ", res->sa.s + res->logger.destination) ;
    }

    char write[strlen(dst) + 10] ;

    /** run script */
    if (!file_write_unsafe(dst, "run", res->sa.s + res->logger.execute.run.run, strlen(res->sa.s + res->logger.execute.run.run)))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/run.user") ;

    auto_strings(write, dst, "/run") ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;

    /** run.user script */
    if (!file_write_unsafe(dst, "run.user", res->sa.s + res->logger.execute.run.run_user, strlen(res->sa.s + res->logger.execute.run.run_user)))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/run.user") ;

    auto_strings(write, dst, "/run.user") ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
}
