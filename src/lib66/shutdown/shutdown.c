/*
 * shutdown.c
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
#include <string.h>
#include <utmpx.h>

#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/shutdown.h>

#define SHUTDOWN_MAX 4096 // matches the historical shutdown access-control buffer: a file this size or larger is rejected

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

static int is_sep(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ;
}

static int valid_username(char const *user)
{
    if (!user || !user[0])
        return 0 ;

    for (char const *p = user ; *p ; p++)
        if (is_sep(*p) || *p == '#')
            return 0 ;

    return 1 ;
}

static int shutdown_read(strbuf *sb)
{
    log_flow() ;

    ssize_t sz = file_get_size(SHUTDOWN_FILE) ;
    if (sz < 0)
        return errno == ENOENT ? 1 : -1 ;

    if (sz >= SHUTDOWN_MAX)
        return (errno = EFBIG, -1) ;

    if (!strbuf_read_file(sb, SHUTDOWN_FILE))
        return errno == ENOENT ? 1 : -1 ;

    if (sb->len && !sbl_split_string_in_nline(sb))
        return -1 ;

    return 1 ;
}

static int authorized_user(strbuf *sb)
{
    int found = 0 ;

    setutxent() ;

    for (;;) {

        errno = 0 ;
        struct utmpx *utx = getutxent() ;
        if (!utx)
            break ;

        if (utx->ut_type != USER_PROCESS)
            continue ;

        size_t pos = 0 ;
        FOREACH_SBL(sb, pos) {
            char const *line = sb->s + pos ;
            if (valid_username(line) && !strncmp(utx->ut_user, line, UT_NAMESIZE)) {
                found = 1 ;
                break ;
            }
        }

        if (found)
            break ;
    }

    endutxent() ;

    return found ;
}

int shutdown_search(char const *user)
{
    log_flow() ;

    if (!valid_username(user))
        return 0 ;

    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ;
    if (shutdown_read(&sb) < 0)
        return -1 ;

    return sbl_search(&sb, user) >= 0 ? 1 : 0 ;
}

int shutdown_list(strbuf *out)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ;
    if (shutdown_read(&sb) < 0)
        return 0 ;

    size_t pos = 0 ;
    FOREACH_SBL(&sb, pos) {
        char const *line = sb.s + pos ;
        if (valid_username(line) && (!strbuf_cats(out, line) || !strbuf_catb(out, "\n", 1)))
            return 0 ;
    }

    return 1 ;
}

int shutdown_add(char const *user)
{
    log_flow() ;

    if (!valid_username(user))
        return (errno = EINVAL, 0) ;

    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ;
    if (shutdown_read(&sb) < 0)
        return 0 ;

    if (sbl_search(&sb, user) >= 0)
        return 1 ;

    if (!sbl_add(&sb, user) || !sbl_rebuild_nline(&sb))
        return 0 ;

    return file_write_atomic(SHUTDOWN_FILE, sb.s, sb.len) ;
}

int shutdown_remove(char const *user)
{
    log_flow() ;

    if (!valid_username(user))
        return (errno = EINVAL, 0) ;

    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ;
    if (shutdown_read(&sb) < 0)
        return 0 ;

    if (sbl_search(&sb, user) < 0)
        return 1 ;

    if (!sbl_remove(&sb, user))
        return 0 ;

    if (sb.len && !sbl_rebuild_nline(&sb))
        return 0 ;

    return file_write_atomic(SHUTDOWN_FILE, sb.len ? sb.s : "", sb.len) ;
}

int shutdown_isallowed(void)
{
    log_flow() ;

    ssize_t sz = file_get_size(SHUTDOWN_FILE) ;
    if (sz < 0)
        return errno == ENOENT ? 1 : -1 ; // no file: everyone is allowed

    _cleanup_strbuf_ strbuf sb = STRBUF_ZERO ;
    if (shutdown_read(&sb) < 0)
        return -1 ;

    return authorized_user(&sb) ;
}
