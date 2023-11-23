/*
 * parser_clean_runas.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>

#include <skalibs/types.h>

#include <66/parse.h>
#include <66/utils.h>

int parse_clean_runas(char const *str, int idsec, int idkey)
{
    size_t len = strlen(str) ;
    char file[len + 1] ;
    auto_strings(file, str) ;

    char *colon ;
    colon = strchr(file,':') ;

    if (colon) {

        *colon = 0 ;

        uid_t uid ;
        gid_t gid ;
        size_t uid_strlen ;
        size_t gid_strlen ;
        static char uid_str[UID_FMT] ;
        static char gid_str[GID_FMT] ;

        /** on format :gid, get the uid of
         * the owner of the process */
        if (!*file) {

            uid = getuid() ;

        }
        else {

            if (get_uidbyname(file, &uid) == -1) {
                log_warnu("get uid of user: ", file) ;
                parse_error_return(0, 0, idsec, idkey) ;
            }
        }

        uid_strlen = uid_fmt(uid_str,uid) ;
        uid_str[uid_strlen] = 0 ;

        /** on format uid:, get the gid of
         * the owner of the process */
        if (!*(colon + 1)) {

            if (!yourgid(&gid,uid))
                parse_error_return(0, 0, idsec, idkey) ;
        }
        else {

            if (get_gidbygroup(colon + 1,&gid) == -1)
                parse_error_return(0, 0, idsec, idkey) ;
        }
        gid_strlen = gid_fmt(gid_str,gid) ;
        gid_str[gid_strlen] = 0 ;


        auto_strings((char *)str, uid_str, ":", gid_str) ;

    } else {

        int e = errno ;
        errno = 0 ;

        struct passwd *pw = getpwnam(str);

        if (!pw) {

            if (!errno)
                errno = ESRCH ;

            log_warnu("invalid user: ", str) ;
            parse_error_return(0, 0, idsec, idkey) ;
        }

        errno = e ;

    }

    return 1 ;
}
