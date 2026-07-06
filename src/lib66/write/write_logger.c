/*
 * write_logger.c
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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/write.h>
#include <66/utils.h>
#include <66/parse.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

/** Destination -> /var/lib/66/system/service/svc/<name> */

void write_logger(resolve_service_t *res, resolve_service_addon_execute_t *ex, char const *destination, uint8_t force)
{
    log_flow() ;

    uid_t log_uid ;
    gid_t log_gid ;

    char *logrunner = ex->run.runas ? ex->sa.s + ex->run.runas : SS_LOGGER_RUNNER ;

    if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid)) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;
    }

    /** The creation of the log destination directory is made by the 66-execute program */

    char write[strlen(destination) + 10] ;

    /** run script */
    log_trace("write file: ", destination, "/run") ;
    if (!file_write_at(destination, "run", ex->sa.s + ex->run.run, strlen(ex->sa.s + ex->run.run))) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "write: ", destination, "/run.user") ;
    }

    auto_strings(write, destination, "/run") ;

    if (chmod(write, 0755) < 0) {
        parse_cleanup(res, destination, force) ;
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
    }
}
