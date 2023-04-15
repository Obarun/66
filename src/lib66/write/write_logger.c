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

void write_logger(resolve_service_t *res, char const *destination)
{
    log_flow() ;

    uid_t log_uid ;
    gid_t log_gid ;
    uint8_t owner = res->owner ;

    char *logrunner = res->execute.run.runas ? res->sa.s + res->execute.run.runas : SS_LOGGER_RUNNER ;

    if (res->execute.timeout.kill)
        write_uint(destination, "timeout-kill", res->execute.timeout.kill) ;

    if (res->execute.timeout.finish)
        write_uint(destination, "timeout-finish", res->execute.timeout.finish) ;

    /** notification */
    write_uint(destination, "notification-fd", 3) ;

    /** log destination */
    log_trace("create directory: ", res->sa.s + res->logger.destination) ;
    if (!dir_create_parent(res->sa.s + res->logger.destination, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", res->sa.s + res->logger.destination) ;

    if (!owner && ((res->execute.run.build == BUILD_AUTO) || (!res->execute.run.build))) {

        if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid))
            log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;

        if (chown(res->sa.s + res->logger.destination, log_uid, log_gid) == -1)
            log_dieusys(LOG_EXIT_SYS, "chown: ", res->sa.s + res->logger.destination) ;
    }

    char write[strlen(destination) + 10] ;

    /** run script */
    if (!file_write_unsafe(destination, "run", res->sa.s + res->execute.run.run, strlen(res->sa.s + res->execute.run.run)))
        log_dieusys(LOG_EXIT_SYS, "write: ", destination, "/run.user") ;

    auto_strings(write, destination, "/run") ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;

    /** run.user script */
    if (!file_write_unsafe(destination, "run.user", res->sa.s + res->execute.run.run_user, strlen(res->sa.s + res->execute.run.run_user)))
        log_dieusys(LOG_EXIT_SYS, "write: ", destination, "/run.user") ;

    auto_strings(write, destination, "/run.user") ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
}
