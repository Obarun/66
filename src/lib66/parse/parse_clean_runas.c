/*
 * parser_clean_runas.c
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
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>

#include <skalibs/types.h>

#include <66/parse.h>
#include <66/utils.h>

int parse_clean_runas(char const *str, int idsec, int idkey)
{
    log_flow() ;

    size_t len = strlen(str) ;
    char file[len + 1] ;
    auto_strings(file, str) ;

    char *colon ;
    colon = strchr(file,':') ;

    if (colon) {

        *colon = 0 ;

        uid_t uid ;
        gid_t gid ;
        static char uid_str[UID_FMT] ;
        static char gid_str[GID_FMT] ;

        /** on format :gid, get the uid of
         * the owner of the process */
        if (!*file) {

            uid = getuid() ;

        }
        else {

            if (!uint320_scan(file, &uid))
                parse_error_return(0, 3, idsec, list_section_startstop, idkey) ;

            if (!getpwuid(uid)) {
                log_warnu("get uid of user: ", file) ;
                parse_error_return(0, 0, idsec, list_section_startstop, idkey) ;
            }
        }

        uid_str[uid_fmt(uid_str,uid)] = 0 ;

        /** on format uid:, get the gid of
         * the owner of the process */
        if (!*(colon + 1)) {

            if (!yourgid(&gid,uid))
                parse_error_return(0, 0, idsec, list_section_startstop, idkey) ;

        }
        else {
            if (!uint320_scan(colon + 1, &gid))
                parse_error_return(0, 0, idsec, list_section_startstop, idkey) ;
        }

        gid_str[gid_fmt(gid_str, gid)] = 0 ;

        auto_strings((char *)str, uid_str, ":", gid_str) ;

    } else {

        int e = errno ;
        errno = 0 ;
        struct passwd *pw ;

        if (str[0] >= '0' && str[0] <= '9') {

            uid_t uid = -1 ;
            if (!uint320_scan(str, &uid))
                parse_error_return(0, 3, idsec, list_section_startstop, idkey) ;
            pw = getpwuid(uid) ;

        } else {
            pw = getpwnam(str);
        }

        if (!pw) {

            if (!errno)
                errno = ESRCH ;

            log_warnsys("invalid user: ", str) ;
            parse_error_return(0, 0, idsec, list_section_startstop, idkey) ;
        }

        errno = e ;
    }

    return 1 ;
}
