/*
 * service_graph_cache_write.c
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/fd.h>
#include <oblibs/cdb.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/utils.h>

_Static_assert(sizeof(service_graph_vertex_t) == 24, "bump SERVICE_GRAPH_CACHE_FORMAT") ;
_Static_assert(SERVICE_GRAPH_STATE_MAX == 3, "bump SERVICE_GRAPH_CACHE_FORMAT") ;
_Static_assert((SERVICE_GRAPH_EDGE_DECLARED | SERVICE_GRAPH_EDGE_CONTENTS | SERVICE_GRAPH_EDGE_FROM | SERVICE_GRAPH_EDGE_REQUIREDBY | SERVICE_GRAPH_EDGE_LOGGER) == 0x1f, "bump SERVICE_GRAPH_CACHE_FORMAT") ;

static int dir_make(char const *dir, char const *live, uid_t owner)
{
    char graph[strlen(live) + SS_LIVEGRAPH_LEN + 1] ;
    auto_strings(graph, live, SS_LIVEGRAPH) ;
    ssize_t r = scan_mode(graph, FLAGS_DIR) ;

    if (r < 0)
        return (errno = ENOTDIR, -1) ;

    if (!r && errno != ENOENT)
        return -1 ;

    if (!r) {

        if (owner)
            return 0 ;

        log_trace("create directory: ", dir) ;
        if (!dir_create_parent(dir, 0755))
            return -1 ;

        if (chmod(graph, S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
            return -1 ;

        return 1 ;
    }

    if (!dir_create_parent(dir, 0755))
        return -1 ;

    return 1 ;
}


static int cache_make(service_graph_t const *g, int fd, struct stat const *base)
{
    ocdbmaker c = OCDBMAKER_ZERO ;
    char const *key[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_KEYS ;
    strbuf const *block[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_BLOCKS(g) ;
    char format[SERVICE_GRAPH_CACHE_FORMAT_LEN] ;

    service_graph_cache_format(format, base) ;

    if (!ocdb_make_start(&c, fd))
        return -1 ;

    if (!ocdb_make_add(&c, SERVICE_GRAPH_CACHE_KEY_FORMAT, sizeof SERVICE_GRAPH_CACHE_KEY_FORMAT - 1, format, sizeof(format)))
        return -1 ;

    for (uint32_t i = 0 ; i < SERVICE_GRAPH_CACHE_NBLOCK ; i++)
        if (!ocdb_make_add(&c, key[i], (uint32_t)strlen(key[i]), block[i]->s, (uint32_t)block[i]->len))
            return -1 ;

    return ocdb_make_finish(&c) ? 1 : -1 ;
}

int service_graph_cache_write(service_graph_t const *g, uid_t owner, char const *base, char const *live)
{
    log_flow() ;

    char dir[SERVICE_GRAPH_CACHE_DIR_LEN(strlen(live)) + 1 + SS_LIVEGRAPH_FILE_LEN + 1] ;
    char tmp[SS_MAX_PATH_LEN + 1] ;
    _cleanup_strbuf_ strbuf tmpdir = STRBUF_ZERO ;
    struct stat bst ;
    size_t dirlen ;
    int fd, r ;

    if (geteuid() != owner)
        return 0 ;

    if (stat(base, &bst) < 0)
        return -1 ;

    SERVICE_GRAPH_CACHE_DIR(dir, live, owner) ;
    dirlen = strlen(dir) ;

    r = dir_make(dir, live, owner) ;
    if (r <= 0)
        return r ;

    fd = service_graph_cache_dir(dir, owner) ;
    if (fd < 0)
        return errno == EPERM ? 0 : -1 ;

    close_fd(fd) ;

    if (set_livetmp(&tmpdir, owner, live) < 0)
        return -1 ;

    fd = file_tmp_open_at(tmp, sizeof tmp, tmpdir.s, 0600, (uid_t)-1, (gid_t)-1) ;
    if (fd < 0)
        return -1 ;

    if (cache_make(g, fd, &bst) < 0) {
        close_fd(fd) ;
        file_tryunlink(tmp) ;
        return -1 ;
    }

    auto_strings(dir + dirlen, "/", SS_LIVEGRAPH_FILE) ;

    log_trace("write the cache of the graph: ", dir) ;

    return file_tmp_rename_at(fd, tmp, dir) ? 1 : -1 ;
}
