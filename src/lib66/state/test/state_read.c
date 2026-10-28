/*
 * state_read.c
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

/* The one reader of a state record, by effect: state_read_file() says a file
 * missing by 0 ENOENT, a short one by -1 EINVAL; state_read_remote() reads
 * <dir>/status through it; state_read() reads the live copy first, then the
 * one of <base>, and 0 ENOENT when neither exists. Under a private mkdtemp
 * directory. */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <66/state.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>

static char base[64] = "/tmp/66-state-read.XXXXXX" ;

static void test_file(void)
{
    printf("state_read_file: 1, 0 ENOENT, -1 EINVAL for a short record\n") ;

    ss_state_t sta = STATE_ZERO, got = STATE_ZERO ;
    char dir[SS_MAX_PATH_LEN], file[SS_MAX_PATH_LEN] ;

    auto_strings(dir, base, "one") ;
    auto_strings(file, dir, "/", SS_STATUS) ;

    errno = 0 ;
    assert(state_read_file(&got, file) == 0 && errno == ENOENT) ;
    errno = 0 ;
    assert(state_read_remote(&got, dir) == 0 && errno == ENOENT) ;

    sta.issupervised = STATE_FLAGS_TRUE ;
    assert(state_write_remote(&sta, dir)) ;

    assert(state_read_file(&got, file) == 1 && got.issupervised == STATE_FLAGS_TRUE) ;
    got = (ss_state_t)STATE_ZERO ;
    assert(state_read_remote(&got, dir) == 1 && got.issupervised == STATE_FLAGS_TRUE) ;

    assert(file_write(file, "short", 5)) ;
    errno = 0 ;
    assert(state_read_file(&got, file) == -1 && errno == EINVAL) ;
    errno = 0 ;
    assert(state_read_remote(&got, dir) == -1 && errno == EINVAL) ;

    // a path that cannot be read is a system error
    char fdir[SS_MAX_PATH_LEN] ;
    auto_strings(fdir, file, "/x") ;
    errno = 0 ;
    assert(state_read_file(&got, fdir) == -1 && errno == ENOTDIR) ;
}

static void test_service(void)
{
    printf("state_read: the live copy first, then the one of the parse, 0 ENOENT for none\n") ;

    ss_state_t sta = STATE_ZERO, got = STATE_ZERO ;
    char live[SS_MAX_PATH_LEN], parsed[SS_MAX_PATH_LEN], pfile[SS_MAX_PATH_LEN] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    auto_strings(live, base, "live") ;
    auto_strings(parsed, base, "parsed") ;
    auto_strings(pfile, parsed, "/", SS_STATUS) ;

    resolve_init(&w) ;
    res.live.statedir = resolve_add_string(&w, live) ;
    res.live.status = resolve_add_string(&w, pfile) ;

    errno = 0 ;
    assert(state_read(&got, &res) == 0 && errno == ENOENT) ;

    sta.issupervised = STATE_FLAGS_FALSE ;
    assert(state_write_remote(&sta, parsed)) ;
    assert(state_read(&got, &res) == 1 && got.issupervised == STATE_FLAGS_FALSE) ;

    sta.issupervised = STATE_FLAGS_TRUE ;
    assert(state_write_remote(&sta, live)) ;
    assert(state_read(&got, &res) == 1 && got.issupervised == STATE_FLAGS_TRUE) ;

    // a live copy short: an error, the copy of the parse is not read in its place
    char lfile[SS_MAX_PATH_LEN] ;
    auto_strings(lfile, live, "/", SS_STATUS) ;
    assert(file_write(lfile, "x", 1)) ;
    errno = 0 ;
    assert(state_read(&got, &res) == -1 && errno == EINVAL) ;

    resolve_free_obj(&w) ;
}

int main(void)
{
    printf("Starting state read test...\n") ;

    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    test_file() ;
    test_service() ;

    assert(dir_destroy(base)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
