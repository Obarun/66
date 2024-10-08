/*
 * write_logger.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

/** Destination -> /var/lib/66/system/service/svc/<name> */

void write_logger(resolve_service_t *res, char const *destination, uint8_t force)
{
    log_flow() ;

    uid_t log_uid ;
    gid_t log_gid ;

    char *logrunner = res->execute.run.runas ? res->sa.s + res->execute.run.runas : SS_LOGGER_RUNNER ;

    if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid)) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;
    }

    if (res->execute.timeout.start) {
        if (!write_uint(destination, "timeout-kill", res->execute.timeout.start)) {
            parse_cleanup(res, destination, force) ;
            log_dieusys(LOG_EXIT_SYS, "write uint file timeout-kill") ;
        }
    }

    if (res->execute.timeout.stop) {
        if (!write_uint(destination, "timeout-finish", res->execute.timeout.stop)) {
            parse_cleanup(res, destination, force) ;
            log_dieusys(LOG_EXIT_SYS, "write uint file timeout-finish") ;
        }
    }

    /** notification */
    if (!write_uint(destination, SS_NOTIFICATION, 3)) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "write uint file ", SS_NOTIFICATION) ;
    }

    /** The creation of the log destination directory is made by the 66-execute program */

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
}
