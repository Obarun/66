/*
 * parse_module_check_dir.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

void parse_module_check_dir(char const *src,char const *dir)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t dirlen = strlen(dir) ;

    char t[srclen + dirlen + 1] ;
    auto_strings(t, src, dir) ;

    r = scan_mode(t,S_IFDIR) ;
    if (r < 0) {
        errno = EEXIST ;
        log_diesys(LOG_EXIT_ZERO, "conflicting format of: ", t) ;
    }

    if (!r)
        if (!dir_create_parent(t, 0755))
            log_dieusys(LOG_EXIT_ZERO, "create directory: ", t) ;
}
