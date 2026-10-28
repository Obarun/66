/*
 * parse_module_check_dir.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>
#include <string.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

int parse_module_check_dir(char const *src, char const *dir)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t dirlen = strlen(dir) ;

    char t[srclen + dirlen + 1] ;
    auto_strings(t, src, dir) ;

    r = scan_mode(t, FLAGS_DIR) ;
    if (r < 0)
        log_warn_return(LOG_EXIT_ZERO, "conflicting format of: ", t, " -- a directory is expected") ;

    if (!r && errno != ENOENT)
        return -1 ;

    if (!r && !dir_create_parent(t, 0755))
        return -1 ;

    return 1 ;
}
