/*
 * write_execute_scripts.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <errno.h>
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
    if (!file_write_at(dst, file, run, FAKELEN) || chmod(write, 0755) < 0)
        return -1 ;

    if (runas) {

        size_t len = strlen(runas) ;
        uid_t uid ;
        gid_t gid ;

        char file[len + 1] ;
        auto_strings(file, runas) ;

        char *colon = file + str_search(file, ':') ;

        if (*colon) {

            if (!uid_parse_strict(file, &uid) || !gid_parse_strict(colon + 1, &gid))
                return (errno = EINVAL, -1) ;

        } else {

            if (!youruid(&uid, runas) ||
                !yourgid(&gid, uid))
                return -1 ;
        }

        if (chown(write, uid, gid) < 0)
            return -1 ;
    }

    return 1 ;
}
