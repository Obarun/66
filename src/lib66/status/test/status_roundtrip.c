/*
 * status_roundtrip.c
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

/* Tests for the native status record: pack/unpack is an identity, write/read is
 * an identity, an absent file reads as a clean DOWN (return 1), and a file whose
 * length differs from STATUS_STATE_SIZE or whose version is unknown is rejected
 * and re-inited to service_status_zero (return 0). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/files.h>

#include <66/status.h>

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

static char tdir[] = "/tmp/status_rt_XXXXXX" ;

/* a non-trivial reference record exercising every field. */
static service_status_t reference(void)
{
    service_status_t st = STATUS_ZERO ;
    st.state = STATUS_STATE_UP ;
    st.result = STATUS_RESULT_EXITED ;
    st.who = STATUS_WHO_USER ;
    st.pid = 4242 ;
    st.code = 0x0102abcd ;
    st.stamp.tv_sec = 1700000000 ; st.stamp.tv_nsec = 123456789 ;
    st.readystamp.tv_sec = 1700000005 ; st.readystamp.tv_nsec = 987654321 ;
    st.window_start.tv_sec = 87654 ; st.window_start.tv_nsec = 111 ;
    st.ndeaths = 3 ;
    return st ;
}

static int status_equal(service_status_t const *a, service_status_t const *b)
{
    return a->version == b->version
        && a->state == b->state
        && a->result == b->result
        && a->who == b->who
        && a->pid == b->pid
        && a->code == b->code
        && a->stamp.tv_sec == b->stamp.tv_sec
        && a->stamp.tv_nsec == b->stamp.tv_nsec
        && a->readystamp.tv_sec == b->readystamp.tv_sec
        && a->readystamp.tv_nsec == b->readystamp.tv_nsec
        && a->window_start.tv_sec == b->window_start.tv_sec
        && a->window_start.tv_nsec == b->window_start.tv_nsec
        && a->ndeaths == b->ndeaths ;
}

/* pack -> unpack is an identity on every field. */
static void test_pack_identity(void)
{
    TEST_START("test_pack_identity") ;

    service_status_t in = reference() ;
    char pack[STATUS_STATE_SIZE] ;
    service_status_t out = STATUS_ZERO ;

    status_pack(pack, &in) ;
    status_unpack(pack, &out) ;

    TEST_ASSERT(status_equal(&in, &out), "pack/unpack mismatch") ;
    TEST_END() ;
}

/* write -> read is an identity on disk. */
static void test_write_read_identity(void)
{
    TEST_START("test_write_read_identity") ;

    char p[512] ;
    sprintf(p, "%s/status", tdir) ;

    service_status_t in = reference() ;
    service_status_t out = STATUS_ZERO ;

    TEST_ASSERT(status_write(&in, p) == 1, "write failed") ;
    TEST_ASSERT(status_read(&out, p) == 1, "read failed") ;
    TEST_ASSERT(status_equal(&in, &out), "write/read mismatch") ;

    /* the on-disk file is exactly STATUS_STATE_SIZE bytes. */
    char buf[STATUS_STATE_SIZE + 8] ;
    ssize_t r = file_read(p, buf, sizeof buf) ;
    TEST_ASSERT(r == STATUS_STATE_SIZE, "on-disk record has the wrong size") ;

    unlink(p) ;
    TEST_END() ;
}

/* absent file -> clean DOWN, return 1. */
static void test_absent_is_down(void)
{
    TEST_START("test_absent_is_down") ;

    char p[512] ;
    sprintf(p, "%s/nope", tdir) ;

    service_status_t out = reference() ; // pre-dirtied to prove the re-init
    errno = 0 ;
    int rc = status_read(&out, p) ;

    TEST_ASSERT(rc == 1, "absent file should return 1 (DOWN nominal)") ;
    TEST_ASSERT(status_equal(&out, &service_status_zero), "absent should re-init to zero") ;
    TEST_ASSERT(out.state == STATUS_STATE_DOWN, "absent state should be DOWN") ;
    TEST_END() ;
}

/* wrong size -> reject, return 0, re-init. */
static void test_wrong_size_rejected(void)
{
    TEST_START("test_wrong_size_rejected") ;

    char p[512] ;
    sprintf(p, "%s/status", tdir) ;

    char junk[20] ;
    memset(junk, 0xAB, sizeof junk) ;
    junk[0] = (char)STATUS_VERSION ; // valid version, wrong length
    TEST_ASSERT(file_write_atomic(p, junk, sizeof junk) == 1, "junk write failed") ;

    service_status_t out = reference() ;
    int rc = status_read(&out, p) ;

    TEST_ASSERT(rc == 0, "short file should return 0") ;
    TEST_ASSERT(status_equal(&out, &service_status_zero), "short file should re-init to zero") ;

    /* oversized file is rejected too. */
    char big[STATUS_STATE_SIZE + 8] ;
    memset(big, 0, sizeof big) ;
    big[0] = (char)STATUS_VERSION ;
    TEST_ASSERT(file_write_atomic(p, big, sizeof big) == 1, "big write failed") ;

    out = reference() ;
    rc = status_read(&out, p) ;
    TEST_ASSERT(rc == 0, "oversized file should return 0") ;
    TEST_ASSERT(status_equal(&out, &service_status_zero), "oversized should re-init to zero") ;

    unlink(p) ;
    TEST_END() ;
}

/* unknown version -> reject, return 0, re-init. */
static void test_unknown_version_rejected(void)
{
    TEST_START("test_unknown_version_rejected") ;

    char p[512] ;
    sprintf(p, "%s/status", tdir) ;

    service_status_t in = reference() ;
    char pack[STATUS_STATE_SIZE] ;
    status_pack(pack, &in) ;
    pack[0] = (char)0xFF ; // bogus version, correct length
    TEST_ASSERT(file_write_atomic(p, pack, STATUS_STATE_SIZE) == 1, "write failed") ;

    service_status_t out = reference() ;
    int rc = status_read(&out, p) ;

    TEST_ASSERT(rc == 0, "unknown version should return 0") ;
    TEST_ASSERT(status_equal(&out, &service_status_zero), "unknown version should re-init to zero") ;

    unlink(p) ;
    TEST_END() ;
}

int main(void)
{
    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== status round-trip test suite ===\n\n") ;

    test_pack_identity() ;
    test_write_read_identity() ;
    test_absent_is_down() ;
    test_wrong_size_rejected() ;
    test_unknown_version_rejected() ;

    rmdir(tdir) ;
    printf("\nAll tests passed.\n") ;
    return 0 ;
}
