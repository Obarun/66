/*
 * symlink_atomic.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/* Tests for symlink_atomic: the created link points to exactly @target (byte
 * for byte), an existing symlink or regular file at @name is replaced, the swap
 * leaves no temporary residue in the directory, and a failure (non-existent
 * parent dir) leaves any previous @name untouched. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/fd.h>

#include <66/symlink.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

static char tdir[] = "/tmp/oblibs_sa_XXXXXX" ;

/* count entries in tdir excluding . and .. */
static int count_entries(void)
{
    DIR *d = opendir(tdir) ;
    TEST_ASSERT(d != NULL, "opendir failed") ;
    int n = 0 ;
    struct dirent *de ;
    while ((de = readdir(d))) {
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
            n++ ;
    }
    closedir(d) ;
    return n ;
}

/* assert the symlink @path points to exactly @target (byte for byte) */
static void assert_points_to(char const *path, char const *target)
{
    char buf[4096] ;
    ssize_t r = readlink(path, buf, sizeof buf - 1) ;
    TEST_ASSERT(r >= 0, "readlink failed") ;
    buf[r] = 0 ;
    TEST_ASSERT((size_t)r == strlen(target), "link target length mismatch") ;
    TEST_ASSERT(memcmp(buf, target, r) == 0, "link target content mismatch") ;
}

/* fresh symlink: name does not exist yet (the fast path). */
static void test_create(void)
{
    TEST_START("test_create") ;
    char p[512] ;
    sprintf(p, "%s/link", tdir) ;

    TEST_ASSERT(symlink_atomic("/some/target", p) == 1, "create failed") ;
    assert_points_to(p, "/some/target") ;
    TEST_ASSERT(count_entries() == 1, "unexpected residue after create") ;

    file_tryunlink(p) ;
    TEST_END() ;
}

/* replace an existing symlink: target is swapped, only one entry remains. */
static void test_replace_symlink(void)
{
    TEST_START("test_replace_symlink") ;
    char p[512] ;
    sprintf(p, "%s/link", tdir) ;

    TEST_ASSERT(symlink_atomic("/old/target", p) == 1, "first create failed") ;
    assert_points_to(p, "/old/target") ;

    TEST_ASSERT(symlink_atomic("/new/longer/target", p) == 1, "replace failed") ;
    assert_points_to(p, "/new/longer/target") ;
    TEST_ASSERT(count_entries() == 1, "temp residue left after replace") ;

    file_tryunlink(p) ;
    TEST_END() ;
}

/* replace a regular file: rename() swaps it for the symlink, no residue. */
static void test_replace_regular(void)
{
    TEST_START("test_replace_regular") ;
    char p[512] ;
    sprintf(p, "%s/link", tdir) ;

    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
    TEST_ASSERT(fd >= 0, "create regular file failed") ;
    close_fd(fd) ;

    TEST_ASSERT(symlink_atomic("/target", p) == 1, "replace regular failed") ;
    assert_points_to(p, "/target") ;
    TEST_ASSERT(count_entries() == 1, "temp residue left after replacing regular") ;

    file_tryunlink(p) ;
    TEST_END() ;
}

/* failure: parent dir does not exist -> fail, errno set, nothing created. */
static void test_fail_no_dir(void)
{
    TEST_START("test_fail_no_dir") ;
    char p[512] ;
    sprintf(p, "%s/nope/link", tdir) ;

    errno = 0 ;
    TEST_ASSERT(symlink_atomic("/target", p) == 0, "should fail on missing dir") ;
    TEST_ASSERT(errno != 0, "errno should be set on failure") ;
    TEST_ASSERT(count_entries() == 0, "nothing should have been created") ;

    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== symlink_atomic test suite ===\n\n") ;

    test_create() ;
    test_replace_symlink() ;
    test_replace_regular() ;
    test_fail_no_dir() ;

    rmdir(tdir) ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
