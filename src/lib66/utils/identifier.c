/*
 * identifier.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <grp.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

identifier_table_t identifier_table[] = {

    { .ident = "@I", .func = &identifier_replace_instance },
    { .ident = "@U", .func = &identifier_replace_username },
    { .ident = "@u", .func = &identifier_replace_useruid },
    { .ident = "@G", .func = &identifier_replace_usergroup },
    { .ident = "@g", .func = &identifier_replace_usergid },
    { .ident = "@H", .func = &identifier_replace_home },
    { .ident = "@S", .func = &identifier_replace_shell },
    { .ident = "@R", .func = &identifier_replace_runtime },
    { .ident = 0 }
} ;

static int identifier_get_name(char *store)
{
    uid_t uid = getuid() ;

    if (!uid) {
        memcpy(store, "root", 4) ;
        store[4] = 0 ;
    } else {
        int e = errno ;
        errno = 0 ;
        struct passwd *pw = getpwuid(uid);
        if (!pw) {
            if (!errno) errno = ESRCH ;
            return 0 ;
        }
        size_t len = strlen(pw->pw_name) ;
        memcpy(store, pw->pw_name, len) ;
        store[len] = 0 ;
        errno = e ;
    }
    return 1 ;
}

int identifier_replace_instance(char *store, const char *rid)
{
    ssize_t r = get_len_until(rid,'@'), more = 1 ;
    /** identifier are used in any kind
     * of service not just instantiated one.
     * Make distinction between them. */
    if (r < 0) {
        r = 0 ;
        more = 0 ;
    }
    size_t len = strlen(rid + r + more) ;
    memcpy(store, rid + r + more, len) ;
    store[len] = 0 ;
    return 1 ;
}

int identifier_replace_username(char *store, const char *rid)
{
    return identifier_get_name(store) ;
} ;

int identifier_replace_useruid(char *store, const char *rid)
{
    store[uid_fmt(store, getuid())] = 0 ;
    return 1 ;
} ;

int identifier_replace_usergid(char *store, const char *rid)
{
    uid_t uid = getuid() ;

    if (!uid) {
        store[0] = '0' ;
        store[1] = '\0' ;
    } else {
        store[gid_fmt(store, getgid())] = 0 ;
    }
    return 1 ;
} ;

int identifier_replace_usergroup(char *store, const char *rid)
{
    uid_t uid = getuid() ;

    if (!uid) {
        auto_strings(store, "root") ;
    } else {
        int e = errno ;
        errno = 0 ;
        struct group *gr = getgrgid(getgid());
        if (!gr) {
            if (!errno) errno = ESRCH ;
            return 0 ;
        }
        size_t len = strlen(gr->gr_name) ;
        memcpy(store, gr->gr_name, len) ;
        store[len] = 0 ;
        errno = e ;
    }
    return 1 ;

} ;

int identifier_replace_home(char *store, const char *rid)
{
    uid_t uid = getuid() ;

    if (!uid) {
        auto_strings(store, "/root") ;
    } else {
        int e = errno ;
        errno = 0 ;
        struct passwd *pw = getpwuid(getuid());
        if (!pw) {
            if (!errno) errno = ESRCH ;
            return 0 ;
        }
        size_t len = strlen(pw->pw_dir) ;
        memcpy(store, pw->pw_dir, len) ;
        store[len] = 0 ;
        errno = e ;
    }
    return 1 ;
}

int identifier_replace_shell(char *store, const char *rid)
{
    int e = errno ;
    errno = 0 ;
    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    size_t len = strlen(pw->pw_shell) ;
    memcpy(store, pw->pw_shell, len) ;
    store[len] = 0 ;
    errno = e ;
    return 1 ;
}

int identifier_replace_runtime(char *store, const char *rid)
{
    uid_t uid = getuid() ;
    char runtime[10 + UID_FMT + 1] ; // SS_MAX_PATH should be sufficient

    if (!identifier_replace_useruid(runtime, 0))
        return 0 ;

    if (!uid) {
        auto_strings(store, "/run") ;
    } else {
        auto_strings(store, "/run/user/", runtime) ;
    }
    return 1 ;
}

int identifier_replace(stralloc *sasv, char const *svname)
{
    size_t pos = 0 ;
    int r = 0 ;
    char store[SS_MAX_PATH_LEN] ;
    _alloc_sa_(sa) ;

    if (!stralloc_copyb(&sa, sasv->s, sasv->len) || !stralloc_0(&sa))
        return 0 ;

    sa.len-- ;

    while(identifier_table[pos].ident) {

        r = (*identifier_table[pos].func)(store, svname) ;
        if (!r)
            return 0 ;

        log_trace("replacing identifier: ", identifier_table[pos].ident, " by: ", store) ;
        if (!sastr_replace_g(&sa, identifier_table[pos].ident, store))
            log_warnu_return(LOG_EXIT_ZERO, "replace regex character: ", identifier_table[pos].ident, " by: ", store," for service: ", svname) ;

        pos++ ;
    }

    sasv->len = 0 ;

    return auto_stra(sasv, sa.s) ;
}
