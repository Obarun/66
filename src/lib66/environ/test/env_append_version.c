/*
 * env_append_version.c
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

/* env_append_version(), by effect: the directory of a version that exists is
 * composed and answered 1; a version missing, or a file that is no directory,
 * is answered 0, the version does not exist, and is told apart from a system
 * error (-1, not provoked here: it needs the allocation to fail). Under a
 * private mkdtemp directory. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/log.h>

#include <66/environ.h>

static char conf[64] = "/tmp/66-env-append-version.XXXXXX" ;

int main(void)
{
    printf("Starting env append version test...\n") ;

    VERBOSITY = 0 ;
    assert(mkdtemp(conf)) ;

    char v1[128], file[128], want[128] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    auto_strings(v1, conf, "/1.0.0") ;
    auto_strings(file, conf, "/2.0.0") ;
    assert(dir_create_parent(v1, 0755)) ;
    assert(file_write(file, "x", 1)) ;

    printf("a version that exists is composed and answered 1\n") ;
    assert(env_append_version(&sa, conf, "1.0.0") == 1) ;
    auto_strings(want, conf, "/1.0.0") ;
    assert(!strcmp(sa.s, want)) ;

    printf("a version missing is answered 0\n") ;
    sa.len = 0 ;
    assert(env_append_version(&sa, conf, "0.0.0.1") == 0) ;

    printf("a version that is no directory is answered 0\n") ;
    sa.len = 0 ;
    assert(env_append_version(&sa, conf, "2.0.0") == 0) ;

    assert(unlink(file) == 0) ;
    assert(rmdir(v1) == 0) ;
    assert(rmdir(conf) == 0) ;

    printf("env append version test passed\n") ;

    return 0 ;
}
