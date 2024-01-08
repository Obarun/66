/*
 * write_execute_scripts.c
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
#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/types.h>

#include <66/utils.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

int write_execute_scripts(char const *file, char const *contents, char const *dst, char const *runas)
{

    log_flow() ;

    char write[strlen(dst) + 1 + strlen(file) + 1] ;

    auto_strings(write, dst, "/", file) ;

    char run[strlen(contents) + 1] ;
    auto_strings(run, contents) ;

    log_trace("write file: ", dst, "/", file) ;
    if (!file_write_unsafe(dst, file, run, FAKELEN))
        log_warnusys_return(LOG_EXIT_ZERO, "write: ", dst, "/", file) ;

    if (chmod(write, 0755) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chmod", write) ;

    if (runas) {

        size_t len = strlen(runas) ;
        uid_t uid ;
        gid_t gid ;

        char file[len + 1] ;
        auto_strings(file, runas) ;

        char *colon ;
        colon = strchr(file,':') ;

        if (colon) {

            if (!uid0_scan(file, &uid))
                log_warnu_return(LOG_EXIT_ZERO,"get uid of: ", runas) ;

            if (!gid0_scan(colon + 1, &gid))
                log_warnu_return(LOG_EXIT_ZERO,"get gid of: ", runas) ;

        } else {

            if (!youruid(&uid, runas) ||
                !yourgid(&gid, uid))
                log_warnu_return(LOG_EXIT_ZERO,"get uid and gid of: ", runas) ;
        }

        if (chown(write, uid, gid) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "chown: ", write) ;
    }

    return 1 ;
}
