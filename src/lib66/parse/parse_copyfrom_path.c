/*
 * parse_copyfrom_path.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <66/constants.h>
#include <66/parse.h>

int parse_copyfrom_path(char *src, char *dname, char const *frontend, char const *what)
{
    log_flow() ;

    ssize_t r ;
    size_t flen = strlen(frontend), wlen = strlen(what) ;
    char dir[flen + 1] ;
    char path[flen + wlen + 1] ;

    if (wlen > SS_MAX_PATH_LEN)
        return (errno = ENAMETOOLONG, -1) ;

    if (!ob_dirname(dir, frontend))
        return -1 ;

    if (what[0] == '/') {

        auto_strings(src, what) ;

        if (!ob_basename(dname, what))
            return -1 ;

    } else if (what[0] == '.' && (what[1] == '/' || (what[1] == '.' && what[2] == '/'))) {

        auto_strings(path, dir, what) ;

        char *real = realpath(path, 0) ;
        if (!real)
            return errno == ENOENT || errno == ENOTDIR ? 0 : -1 ;

        if (strlen(real) > SS_MAX_PATH_LEN) {
            free(real) ;
            return (errno = ENAMETOOLONG, -1) ;
        }

        auto_strings(src, real) ;
        free(real) ;

        if (!ob_basename(dname, what))
            return -1 ;

    } else {

        if (strlen(dir) + wlen > SS_MAX_PATH_LEN)
            return (errno = ENAMETOOLONG, -1) ;

        auto_strings(src, dir, what) ;
        auto_strings(dname, what) ;
    }

    r = scan_mode(src, FLAGS_DIR) ;
    if (r > 0)
        return 1 ;

    if (!r && errno != ENOENT)
        return -1 ;

    r = scan_mode(src, FLAGS_REG) ;
    if (r > 0)
        return 1 ;

    // absent is ENOENT, any other 0 a system error
    if (!r)
        return errno == ENOENT ? 0 : -1 ;

    return (errno = ENOTSUP, -1) ;
}
