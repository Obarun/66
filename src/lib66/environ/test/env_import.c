/*
 * env_import.c
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

/* env_import_version_file(), the one rule of the import of a configuration,
 * by effect: an older version is imported into the destination given, each file
 * at the mode of its source, the file of the service itself left; the same
 * version or a newer one imports nothing; versions that do not compare are 0.
 * Under a private mkdtemp directory. */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/log.h>

#include <66/constants.h>
#include <66/environ.h>
#include <66/enum_parser.h>

static char conf[64] = "/tmp/66-env-import.XXXXXX" ;

static void put(char const *dir, char const *name, char const *content, mode_t mode)
{
    char file[256] ;
    auto_strings(file, dir, "/", name) ;
    assert(file_write(file, content, strlen(content))) ;
    assert(chmod(file, mode) == 0) ;
}

static int has(char const *dir, char const *name, char const *content)
{
    char file[256], buf[64] ;
    auto_strings(file, dir, "/", name) ;
    ssize_t r = file_read(file, buf, sizeof(buf) - 1) ;
    if (r < 0)
        return 0 ;
    buf[r] = 0 ;
    return !strcmp(buf, content) ;
}

int main(void)
{
    printf("Starting env import test...\n") ;

    VERBOSITY = 0 ;
    assert(mkdtemp(conf)) ;

    char v1[128], v2[128], stage[128], file[256] ;
    struct stat st ;

    auto_strings(v1, conf, "/1.0.0") ;
    auto_strings(v2, conf, "/2.0.0") ;
    auto_strings(stage, conf, "/stage") ;
    assert(dir_create_parent(v1, 0755) && dir_create_parent(v2, 0755) && dir_create_parent(stage, 0755)) ;

    put(v1, "KEY", "old", 0640) ;
    put(v1, ".svc", "service", 0644) ;
    put(v2, ".svc", "new service", 0644) ;

    printf("an older version is imported into the destination given\n") ;
    assert(env_import_version_file("svc", conf, "1.0.0", "2.0.0", v2, E_PARSER_TYPE_CLASSIC) == 1) ;
    assert(has(v2, "KEY", "old") && has(v2, ".svc", "new service")) ;
    auto_strings(file, v2, "/KEY") ;
    assert(stat(file, &st) == 0 && (st.st_mode & 07777) == 0640) ;

    printf("a staging directory as destination\n") ;
    assert(env_import_version_file("svc", conf, "1.0.0", "2.0.0", stage, E_PARSER_TYPE_CLASSIC) == 1) ;
    assert(has(stage, "KEY", "old") && !has(stage, ".svc", "service")) ;

    printf("the same version, a newer one: nothing imported\n") ;
    put(v1, "KEY", "changed", 0640) ;
    assert(env_import_version_file("svc", conf, "1.0.0", "1.0.0", v2, E_PARSER_TYPE_CLASSIC) == 1) ;
    assert(env_import_version_file("svc", conf, "2.0.0", "1.0.0", v1, E_PARSER_TYPE_CLASSIC) == 1) ;
    assert(has(v2, "KEY", "old")) ;

    printf("versions that do not compare: 0\n") ;
    {
        char toolong[SS_SERVICE_VERSION_MAXLEN + 2] ;
        memset(toolong, '1', sizeof(toolong) - 1) ;
        toolong[sizeof(toolong) - 1] = 0 ;
        assert(env_import_version_file("svc", conf, toolong, "2.0.0", v2, E_PARSER_TYPE_CLASSIC) == 0) ;
    }

    printf("a source that cannot be listed: -1\n") ;
    errno = 0 ;
    assert(env_import_version_file("svc", conf, "0.1.0", "2.0.0", v2, E_PARSER_TYPE_CLASSIC) == -1 && errno == ENOENT) ;

    assert(dir_destroy(conf)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
