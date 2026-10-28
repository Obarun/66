/*
 * service_frontend_path.c
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

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/service.h>

static void compute_exclude(char const **nexclude, char const **oexclude, uint8_t exlen, uid_t owner)
{
    log_flow() ;

    uint8_t pos = 0 ;

    if (exlen) {
        for (; pos < exlen ; pos++)
            nexclude[pos] = oexclude[pos] ;

        if (owner)
            nexclude[pos] = 0 ;
    } else
        nexclude[0] = 0 ;

    if (!owner) {
        if (!exlen) {
            nexclude[0] = "user" ;
            nexclude[1] = 0 ;
        } else {
            nexclude[pos++] = "user" ;
            nexclude[pos] = 0 ;
        }
    }
}

static int frontend_dirs(char const **dir, char *home, uid_t owner)
{
    uint8_t ndir = 0 ;

    if (!owner) {

        dir[ndir++] = SS_SERVICE_ADMDIR ;
        dir[ndir++] = SS_SERVICE_SYSDIR ;

        return ndir ;
    }

    if (!set_ownerhome_stack(home))
        return -1 ;

    auto_strings(home + strlen(home), SS_SERVICE_USERDIR) ;

    dir[ndir++] = home ;
    dir[ndir++] = SS_SERVICE_ADMDIR_USER ;
    dir[ndir++] = SS_SERVICE_SYSDIR_USER ;

    return ndir ;
}

static size_t dir_end(strbuf *list, size_t pos, char const *dir)
{
    size_t len = strlen(dir) ;

    FOREACH_SBL(list, pos)
        if (strncmp(list->s + pos, dir, len))
            break ;

    return pos ;
}

int service_frontend_walk(strbuf *sasrc, char const *sv, strbuf *list, char const **exclude, uint8_t exlen)
{
    log_flow() ;

    int r, ndir ;
    size_t start = 0 ;
    uid_t owner = getuid() ;
    char const *dir[3] ;
    char const *ex[exlen + 2] ;
    char home[SS_MAX_PATH_LEN + 1 + strlen(SS_SERVICE_USERDIR) + 1] ;

    compute_exclude(ex, exclude, exlen, owner) ;

    ndir = frontend_dirs(dir, home, owner) ;
    if (ndir < 0)
        return -1 ;

    for (int pos = 0 ; pos < ndir ; pos++) {

        size_t end = dir_end(list, start, dir[pos]) ;

        if (end == list->len && start == end) {

            if (service_frontend_list(list, dir[pos], ex) < 0)
                return -1 ;

            end = list->len ;
        }

        r = service_frontend_search(sasrc, sv, list, start, end, ex) ;
        if (r < 0)
            return -1 ;

        if (r)
            return 1 ;

        start = end ;
    }

    return 0 ;
}

int service_frontend_path(strbuf *sasrc, char const *sv, uid_t owner, char const *directory_forced, char const **exclude, uint8_t exlen)
{
    log_flow() ;

    int r, ndir ;
    char const *dir[3] ;
    char const *ex[exlen + 2] ;
    char home[SS_MAX_PATH_LEN + 1 + strlen(SS_SERVICE_USERDIR) + 1] ;

    if (directory_forced) {

        char const *exclude[1] = { 0 } ;

        return service_frontend_src(sasrc, sv, directory_forced, exclude) ;
    }

    compute_exclude(ex, exclude, exlen, owner) ;

    ndir = frontend_dirs(dir, home, owner) ;
    if (ndir < 0)
        return -1 ;

    for (int pos = 0 ; pos < ndir ; pos++) {

        r = service_frontend_src(sasrc, sv, dir[pos], ex) ;
        if (r)
            return r ;
    }

    return 0 ;
}
