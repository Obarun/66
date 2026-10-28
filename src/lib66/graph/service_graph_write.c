/*
 * service_graph_write.c
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

/** The write transaction is a state of the process: service_graph_write_begin(),
 * service_graph_write_try() and service_graph_write_end() share it, so they live in this
 * one file around a static state, and nothing of it is exported. */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/clock.h>
#include <oblibs/directory.h>

#include <66/constants.h>
#include <66/graph.h>
#include <66/utils.h>

#define SYSTEM_WRITE_POLL_MS 10 // the nap between two tries of a contended lock
#define SYSTEM_WRITE_WAIT_SEC 1 // the wait after which a writer says it waits


static struct
{
    int fd ;
    uint32_t depth ; // the begins and tries nested, the last end frees the lock
    uid_t owner ;
    char base[SS_MAX_PATH_LEN + 1] ;
    char live[SS_MAX_PATH_LEN + 1] ;
} state = { .fd = -1, .depth = 0, .owner = 0, .base = { 0 }, .live = { 0 } } ;

static void state_reset(void)
{
    state.fd = -1 ;
    state.depth = 0 ;
    state.owner = 0 ;
    state.base[0] = 0 ;
    state.live[0] = 0 ;
}

static int state_nest(uid_t owner, char const *base, char const *live)
{
    if (state.owner != owner || strcmp(state.base, base) || strcmp(state.live, live))
        return (errno = EDEADLK, -1) ;

    state.depth++ ;

    return 1 ;
}

static void state_new(int fd, uid_t owner, char const *base, char const *live)
{
    memcpy(state.base, base, strlen(base) + 1) ;
    memcpy(state.live, live, strlen(live) + 1) ;
    state.fd = fd ;
    state.depth = 1 ;
    state.owner = owner ;
}

static int lock_open(char const *base, bool create)
{
    int fd = (int)io_open(base, O_RDONLY | O_DIRECTORY | O_CLOEXEC) ;

    if (fd >= 0 || errno != ENOENT || !create)
        return fd ;

    if (!dir_create_parent(base, 0755))
        return -1 ;

    return (int)io_open(base, O_RDONLY | O_DIRECTORY | O_CLOEXEC) ;
}

static int lock_wait(int fd)
{
    struct timespec now, deadline, nap ;
    int r = lock_fd(fd, 1, 1) ;

    if (r)
        return r ;

    if (!clock_now_mono(&deadline))
        return -1 ;

    clock_addsec(&deadline, &deadline, SYSTEM_WRITE_WAIT_SEC) ;

    for (;;) {

        if (!clock_now_mono(&now))
            return -1 ;

        if (clock_cmp(&now, &deadline) >= 0)
            break ;

        clock_from_ms(&nap, SYSTEM_WRITE_POLL_MS) ;
        clock_add(&nap, &now, &nap) ;

        if (!clock_deepsleep(&nap))
            return -1 ;

        r = lock_fd(fd, 1, 1) ;
        if (r)
            return r ;
    }

    log_trace("waiting for another 66 command to release the lock of the system") ;

    return lock_fd(fd, 1, 0) ;
}

static int tmp_empty(void)
{
    size_t baselen = strlen(state.base) ;
    char tmp[TMPDIR_GETLEN(baselen) + 1] ;

    auto_strings(tmp, state.base, SS_TMPDIR + 1) ;

    log_trace("empty the temporary directory of the system: ", tmp) ;
    if (!dir_destroy(tmp) || !dir_create_parent(tmp, 0755))
        return -1 ;

    return 1 ;
}

static int build(service_graph_t *g, bool cache)
{
    service_graph_free(g) ;

    if (service_graph_build(g, state.base, 0) < 0)
        return -1 ;

    if (cache && service_graph_cache_write(g, state.owner, state.base, state.live) < 0)
        return 0 ;

    return 1 ;
}

int service_graph_write_begin(uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    int fd ;

    if (state.depth) {

        if (state_nest(owner, base, live) < 0)
            return -1 ;

        if (service_graph_cache_drop(state.owner, state.live) < 0) {
            state.depth-- ;
            return -1 ;
        }

        return 1 ;
    }

    if (strlen(base) > SS_MAX_PATH_LEN || strlen(live) > SS_MAX_PATH_LEN)
        return (errno = ENAMETOOLONG, -1) ;

    fd = lock_open(base, true) ;
    if (fd < 0)
        return -1 ;

    if (lock_wait(fd) < 0) {
        close_fd(fd) ;
        return -1 ;
    }

    state_new(fd, owner, base, live) ;

    if (service_graph_cache_drop(state.owner, state.live) < 0 || tmp_empty() < 0) {
        close_fd(state.fd) ;
        state_reset() ;
        return -1 ;
    }

    return 1 ;
}

int service_graph_write_try(uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    int fd, r ;

    if (state.depth)
        return state_nest(owner, base, live) ;

    if (strlen(base) > SS_MAX_PATH_LEN || strlen(live) > SS_MAX_PATH_LEN)
        return (errno = ENAMETOOLONG, -1) ;

    fd = lock_open(base, false) ;
    if (fd < 0)
        return errno == ENOENT ? 0 : -1 ;

    r = lock_fd(fd, 1, 1) ;
    if (r <= 0) {
        close_fd(fd) ;
        return r ;
    }

    state_new(fd, owner, base, live) ;

    return 1 ;
}

int service_graph_write_end(service_graph_t *g)
{
    log_flow() ;

    if (!state.depth)
        return (errno = EINVAL, -1) ;

    bool last = !--state.depth ;
    int r = 1 ;

    if (g) {

        r = build(g, last) ;

    } else if (last) {

        service_graph_t tmp = SERVICE_GRAPH_ZERO ;

        r = build(&tmp, true) > 0 ;

        int e = errno ;
        service_graph_free(&tmp) ;
        errno = e ;
    }

    if (last) {
        close_fd(state.fd) ;
        state_reset() ;
    }

    return r ;
}
