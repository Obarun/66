/*
 * service_graph_cache_read.c
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/fd.h>
#include <oblibs/cdb.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/graph.h>

static int graph_bounds(service_graph_t const *g)
{
    uint32_t nvertex, nedge, depends = 0 ;
    uint32_t const *edge = (uint32_t const *)g->depends.s ;

    if (g->vertex.len % sizeof(service_graph_vertex_t) || g->depends.len % sizeof(uint32_t))
        return 0 ;

    nvertex = service_graph_nvertex(g) ;
    nedge = (uint32_t)(g->depends.len / sizeof(uint32_t)) ;

    if (nvertex > SS_MAX_SERVICE || g->dorigin.len != nedge)
        return 0 ;

    if (nvertex && (!g->strings.len || g->strings.s[g->strings.len - 1]))
        return 0 ;

    for (uint32_t index = 0 ; index < nvertex ; index++) {

        service_graph_vertex_t const *v = service_graph_vertex(g, index) ;

        if (v->name >= g->strings.len || v->ndepends > nedge - depends)
            return 0 ;

        depends += v->ndepends ;
    }

    if (depends != nedge)
        return 0 ;

    for (uint32_t k = 0 ; k < nedge ; k++)
        if (edge[k] >= nvertex)
            return 0 ;

    return 1 ;
}

static int blocks_copy(service_graph_t *g, ocdb const *c)
{
    char const *key[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_KEYS ;
    strbuf *block[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_BLOCKS(g) ;
    ocdb_data data ;

    for (uint32_t i = 0 ; i < SERVICE_GRAPH_CACHE_NBLOCK ; i++) {

        if (ocdb_find(c, &data, key[i], (uint32_t)strlen(key[i])) <= 0)
            return 0 ;

        if (!strbuf_copyb(block[i], data.s, data.len))
            return -1 ;
    }

    return 1 ;
}

static int cache_load(service_graph_t *g, int fd, struct stat const *base)
{
    ocdb c = OCDB_ZERO ;
    ocdb_data data ;
    char format[SERVICE_GRAPH_CACHE_FORMAT_LEN] ;
    int r ;

    if (!ocdb_init_fromfd(&c, fd))
        return 0 ;

    service_graph_cache_format(format, base) ;

    r = ocdb_find(&c, &data, SERVICE_GRAPH_CACHE_KEY_FORMAT, sizeof SERVICE_GRAPH_CACHE_KEY_FORMAT - 1) ;
    if (r <= 0 || data.len != sizeof(format) || memcmp(data.s, format, sizeof(format))) {
        ocdb_free(&c) ;
        return (errno = EINVAL, 0) ;
    }

    r = blocks_copy(g, &c) ;
    ocdb_free(&c) ;

    if (!r)
        errno = EINVAL ;

    return r ;
}

int service_graph_cache_read(service_graph_t *g, uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    char dir[SERVICE_GRAPH_CACHE_DIR_LEN(strlen(live)) + 1] ;
    struct stat bst, st ;
    int dfd, fd, r ;

    SERVICE_GRAPH_CACHE_DIR(dir, live, owner) ;

    if (stat(base, &bst) < 0)
        return 0 ;

    dfd = service_graph_cache_dir(dir, owner) ;
    if (dfd < 0)
        return errno == EPERM ? (errno = EINVAL, 0) : 0 ;

    fd = openat(dfd, SS_LIVEGRAPH_FILE, O_RDONLY | O_NOFOLLOW | O_CLOEXEC) ;
    close_fd(dfd) ;

    if (fd < 0)
        return 0 ;

    if (fstat(fd, &st) < 0) {
        close_fd(fd) ;
        return 0 ;
    }

   if (!S_ISREG(st.st_mode)) {
        close_fd(fd) ;
        return (errno = EINVAL, 0) ;
    }

    r = cache_load(g, fd, &bst) ;
    close_fd(fd) ;

    if (r > 0 && !graph_bounds(g)) {
        r = 0 ;
        errno = EINVAL ;
    }

    if (r > 0)
        r = service_graph_derive(g) ;

    if (r <= 0)
        service_graph_free(g) ;

    return r ;
}
