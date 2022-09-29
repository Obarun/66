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

    int build = !strcmp(res->sa.s + res->logger.execute.run.build, "custom") ? 1 : 0 ;

    char *pmax = 0 ;
    char *pback = 0 ;
    char max[UINT32_FMT] ;
    char back[UINT32_FMT] ;
    char *timestamp = 0 ;
    int itimestamp = SS_LOGGER_TIMESTAMP ;
    char *logrunner = res->logger.execute.run.runas ? res->sa.s + res->logger.execute.run.runas : SS_LOGGER_RUNNER ;

    char dst[strlen(destination) + 1 + strlen(res->sa.s + res->logger.name) + 1] ;
    auto_strings(dst, destination, "/", res->sa.s + res->logger.name) ;

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

    /** timestamp */
    if (res->logger.timestamp != 3)
        timestamp = res->logger.timestamp == TIME_NONE ? "" : res->logger.timestamp == TIME_ISO ? "T" : "t" ;
    else
        timestamp = itimestamp == TIME_NONE ? "" : itimestamp == TIME_ISO ? "T" : "t" ;

    /** backup */
    if (res->logger.backup) {

        back[uint32_fmt(back,res->logger.backup)] = 0 ;
        pback = back ;

    } else
        pback = "3" ;

    /** file size */
    if (res->logger.maxsize) {

        max[uint32_fmt(max,res->logger.maxsize)] = 0 ;
        pmax = max ;

    } else
        pmax = "1000000" ;

    /** log destination */
    size_t namelen = strlen(res->sa.s + res->name) ;
    size_t syslen = res->owner ? strlen(res->sa.s + res->path.home) + 1 + strlen(SS_LOGGER_USERDIR) : strlen(SS_LOGGER_SYSDIR) ;
    size_t dstlen = res->logger.destination ? strlen(res->sa.s + res->logger.destination) : strlen(SS_LOGGER_SYSDIR) ;

    char dstlog[syslen + dstlen + namelen + 1] ;

    if (!res->logger.destination) {

        if (res->owner)

            auto_strings(dstlog, res->sa.s + res->path.home, "/", SS_LOGGER_USERDIR, res->sa.s + res->name) ;

        else

            auto_strings(dstlog, SS_LOGGER_SYSDIR, res->sa.s + res->name) ;

    } else {

        auto_strings(dstlog, res->sa.s + res->logger.destination) ;
    }

    if (!dir_create_parent(dstlog, 0755))
        log_dieusys(LOG_EXIT_SYS, "create directory: ", dstlog) ;

    if (!owner && ((res->logger.execute.run.build == BUILD_AUTO) || (!res->logger.execute.run.build))) {

        if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid))
            log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;

        if (chown(dstlog, log_uid, log_gid) == -1)
            log_dieusys(LOG_EXIT_SYS, "chown: ", dstlog) ;
    }

    {
        /** dst/run file */
        char *shebang = res->logger.execute.run.shebang ? res->sa.s + res->logger.execute.run.shebang : "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;

        char run[strlen(shebang) + strlen(res->sa.s + res->live.fdholderdir) + SS_FDHOLDER_PIPENAME_LEN +  strlen(res->sa.s + res->logger.name) + strlen(S6_BINPREFIX) + strlen(res->sa.s + res->logger.execute.run.runas) + 53 + 1] ;

        auto_strings(run, \
                    shebang, \
                    "s6-fdholder-retrieve ", \
                    res->sa.s + res->live.fdholderdir, "/s ", \
                    "\"" SS_FDHOLDER_PIPENAME "r-", \
                    res->sa.s + res->logger.name, "\"\n") ;

        /** runas */
        if (!res->owner && res->logger.execute.run.runas)
            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", res->sa.s + res->logger.execute.run.runas, "\n") ;

        auto_strings(run + FAKELEN, "./run.user\n") ;

        if (!file_write_unsafe(dst, "run", run, FAKELEN))
            log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/run.user") ;

        char write[strlen(dst) + 5] ;
        auto_strings(write, dst, "/run") ;

        if (chmod(write, 0755) < 0)
            log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
    }

    if (!build) {

        char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
        size_t shebanglen = strlen(shebang) ;

        char run[shebanglen + strlen(S6_BINPREFIX) + strlen(logrunner) + strlen(pback) + strlen(timestamp) + strlen(pmax) + strlen(dstlog) + 45 + 1] ;

        auto_strings(run, \
                    shebang, \
                    "fdmove -c 2 1\n") ;

        if (!owner)
            auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", logrunner, "\n") ;

        auto_strings(run + FAKELEN, "s6-log ") ;

        if (SS_LOGGER_NOTIFY)
            auto_strings(run + FAKELEN, "-d3 ") ;

        auto_strings(run + FAKELEN, "n", pback, " ") ;

        if (res->logger.timestamp < TIME_NONE)
            auto_strings(run + FAKELEN, timestamp, " ") ;

        auto_strings(run + FAKELEN, "s", pmax, " ", dstlog, "\n") ;

        if (!file_write_unsafe(dst, "run.user", run, FAKELEN))
            log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/run.user") ;

        /** notification fd */
        if (SS_LOGGER_NOTIFY)
            write_uint(dst, SS_NOTIFICATION, 3) ;

    } else {

        if (!file_write_unsafe(dst, "run.user", res->sa.s + res->logger.execute.run.run_user, strlen(res->sa.s + res->logger.execute.run.run_user)))
            log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/run.user") ;
    }

    char write[strlen(dst) + 10] ;
    auto_strings(write, dst, "/run.user") ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
}
