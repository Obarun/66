/*
 * graph.h
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_GRAPH_H
#define SS_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/bits.h>
#include <oblibs/graph.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>

struct resolve_hash_s ;
struct selection_service_s ;

#if SS_MAX_SERVICE > UINT32_BITS_MAX
#error "SS_MAX_SERVICE exceeds the capacity of a bitset32_t: a selection could not hold every service"
#endif

#define SERVICE_GRAPH_NOINDEX UINT32_MAX
#define SERVICE_GRAPH_EDGE_DECLARED (1 << 0)
#define SERVICE_GRAPH_EDGE_CONTENTS (1 << 1)
#define SERVICE_GRAPH_EDGE_FROM (1 << 2)
#define SERVICE_GRAPH_EDGE_REQUIREDBY (1 << 3)
#define SERVICE_GRAPH_EDGE_LOGGER (1 << 4)

// the state of a vertex, read from the disk when the graph is built
#define SERVICE_GRAPH_STATE_OK 0 // its resolve file and addons read
#define SERVICE_GRAPH_STATE_ABSENT 1 // no resolve file at all: a name a neighbour declares and nobody parsed
#define SERVICE_GRAPH_STATE_UNREADABLE 2 // a resolve file present that service_read() refuses: itself, or an addon it declares, missing or unreadable
#define SERVICE_GRAPH_STATE_UNORDERED 3 // on a dependency cycle, or depending on one: it has no place in the start order
#define SERVICE_GRAPH_STATE_MAX SERVICE_GRAPH_STATE_UNORDERED
// a vertex is invalid when it is UNREADABLE or UNORDERED, or when its frontend is gone
// (service_graph_frontend_gone); a message names the first cause in this order (contract, Invalid service)

/**
 * @struct service_graph_vertex_s
 * @brief One vertex of the graph of the system.
 *
 * Its edges are a slice of service_graph_s::depends and of
 * service_graph_s::requiredby, each sorted by index, their origins at the same
 * position in dorigin and rorigin.
 *
 * @param name
 * Offset of its name in service_graph_s::strings.
 *
 * @param depends
 * Position of its first edge in service_graph_s::depends.
 *
 * @param ndepends
 * Number of its edges in service_graph_s::depends.
 *
 * @param requiredby
 * Position of its first edge in service_graph_s::requiredby.
 *
 * @param nrequiredby
 * Number of its edges in service_graph_s::requiredby.
 *
 * @param state
 * Its state, SERVICE_GRAPH_STATE_OK ... SERVICE_GRAPH_STATE_MAX.
 */
typedef struct service_graph_vertex_s service_graph_vertex_t, *service_graph_vertex_t_ref ;
struct service_graph_vertex_s
{
    uint32_t name ;
    uint32_t depends ;
    uint32_t ndepends ;
    uint32_t requiredby ;
    uint32_t nrequiredby ;
    uint8_t state ;
} ;

#define SERVICE_GRAPH_VERTEX_ZERO { .name = 0, .depends = 0, .ndepends = 0, .requiredby = 0, .nrequiredby = 0, .state = SERVICE_GRAPH_STATE_OK }

/**
 * @struct service_graph_s
 * @brief The graph of the system of an owner, as every command reads it.
 *
 * Each block is an array held in its own strbuf, which the graph owns: the
 * graph never points into a file or a buffer it does not own, whatever source
 * filled it. Each count is the length of its block divided by the size of an
 * element, read through the inline accessors below. requiredby, the start
 * order and the state SERVICE_GRAPH_STATE_UNORDERED are derived from the rest
 * in one pass.
 *
 * It carries neither the owner nor the base nor anything of the command that
 * reads it: a function that needs them receives them as arguments.
 *
 * @param vertex
 * `service_graph_vertex_t`, one per vertex, sorted by name.
 *
 * @param sort
 * `uint32_t`, the start order, a dependency before what depends on it. It
 * holds every vertex that is not SERVICE_GRAPH_STATE_UNORDERED, and those only.
 *
 * @param depends
 * `uint32_t`, the index at the other end of each edge of a vertex toward what
 * it depends on.
 *
 * @param requiredby
 * `uint32_t`, the index at the other end of each edge toward what depends on a
 * vertex: the exact transpose of depends, kept because the propagation walks
 * both directions on every command.
 *
 * @param dorigin
 * `uint8_t`, the SERVICE_GRAPH_EDGE_* origin of each edge of depends.
 *
 * @param rorigin
 * `uint8_t`, the SERVICE_GRAPH_EDGE_* origin of each edge of requiredby.
 *
 * @param strings
 * The names, each one ending with a zero.
 */
typedef struct service_graph_s service_graph_t, *service_graph_t_ref ;
struct service_graph_s
{
    strbuf vertex ;
    strbuf sort ;
    strbuf depends ;
    strbuf requiredby ;
    strbuf dorigin ;
    strbuf rorigin ;
    strbuf strings ;
} ;

#define SERVICE_GRAPH_ZERO { .vertex = STRBUF_ZERO, .sort = STRBUF_ZERO, .depends = STRBUF_ZERO, .requiredby = STRBUF_ZERO, .dorigin = STRBUF_ZERO, .rorigin = STRBUF_ZERO, .strings = STRBUF_ZERO }

/**
 *
 * Loading
 *
 * */

/**
 * @brief Fill @g with the graph of the system of @owner at @base.
 *
 * The graph is built from the resolve files of @base, each one read by
 * service_read() with every addon it declares: a service the graph says
 * SERVICE_GRAPH_STATE_OK is readable for every command. A resolve file that
 * does not read never makes the load fail: its vertex is
 * SERVICE_GRAPH_STATE_UNREADABLE.
 *
 * The cache of @owner under @live is read when service_graph_cache_read()
 * accepts it; otherwise the graph is built by service_graph_reload(). A graph
 * built while another process holds the lock of the system is used and never
 * written. A reader never waits for a writer. Inside a write transaction of
 * the process, whose begin dropped the cache, the graph is built and the cache
 * left to service_graph_write_end().
 *
 * @param[out] g     Graph to fill. Must be SERVICE_GRAPH_ZERO or released by
 *                   service_graph_free().
 * @param[in]  owner Owner of the system.
 * @param[in]  base  Base directory of the system.
 * @param[in]  live  Runtime directory holding the cache, given by the caller
 *                   as it gives @base (SS_LIVE for a command).
 *
 * @return 1 when @g holds the graph.
 * @return -1 on a system error, errno set, @g left empty.
 *
 * @see service_graph_reload
 */
extern int service_graph_load(service_graph_t *g, uid_t owner, char const *base, char const *live) ;

/**
 * @brief Fill @g with the graph of the system, always from the resolve files.
 *
 * As service_graph_load(), but the cache, when there is one, is not read, and
 * it is written again under the same rule. A command whose
 * service_graph_resolve_load() found a stale graph (it returned 0) calls it
 * once, then draws its selections again: every vertex index and every store
 * drawn from the old graph are void. The vertex that did not read is then
 * SERVICE_GRAPH_STATE_UNREADABLE, and the command refuses it as the contract
 * writes. A cache that cannot be written is said once at verbosity 2
 * (log_warnusys) and @g holds the graph all the same.
 *
 * @param[out] g     Graph to fill. Must be SERVICE_GRAPH_ZERO or released by
 *                   service_graph_free().
 * @param[in]  owner Owner of the system.
 * @param[in]  base  Base directory of the system.
 * @param[in]  live  Runtime directory holding the cache.
 *
 * @return 1 when @g holds the graph.
 * @return -1 on a system error, errno set: @g left empty when the build
 *         failed; @g untouched when the lock could not be tried
 *         (service_graph_write_try() gave -1: ENAMETOOLONG, EDEADLK, or the
 *         errno of the open or the flock).
 *
 * @see service_graph_load
 */
extern int service_graph_reload(service_graph_t *g, uid_t owner, char const *base, char const *live) ;

// release the blocks of @g, which is then SERVICE_GRAPH_ZERO
extern void service_graph_free(service_graph_t *g) ;

/**
 *
 * Construction of the graph of the system. Every command reads the graph
 * through service_graph_load(); only the loading, the parse (which checks what
 * it holds in memory against the system before writing) and the release of a
 * write transaction build it.
 *
 * */

/**
 * @brief Build in @g the graph of the system at @base from its resolve files.
 *
 * Every resolve file of @base is a vertex, every name a Depends, a RequiredBy,
 * an OptsDepends taken, a module or a From names is a vertex too, absent when
 * it has no resolve file. A name a resolve declares is resolved as the parse
 * resolves it: to the service of @overlay that provides it, else through its
 * link in the Provide directory of @base, else taken literally.
 *
 * The services of @overlay, the resolve files a parse holds in memory and has
 * not written, replace their resolve file on disk, or join the system when
 * they have none.
 *
 * The edges are posted from the declarations only, each with its origin: a
 * Depends, an OptsDepends taken and a logger declared; a RequiredBy of a
 * service as a Depends of the service it names, declared and marked
 * SERVICE_GRAPH_EDGE_REQUIREDBY; the contents of a module; the From of a
 * reactor. The state of each vertex is then set, an unreadable resolve before
 * anything else, and the vertexes are put in the start order (Kahn): what is
 * left out of it is SERVICE_GRAPH_STATE_UNORDERED.
 *
 * Each resolve file is read by service_read() with every addon it declares,
 * the one definition of a readable service. A resolve file that does not read
 * never fails the build: its vertex is SERVICE_GRAPH_STATE_UNREADABLE, its own
 * edges unknown, the edges its neighbours declare toward it kept. A listed
 * name whose resolve file service_read() does not find (0, errno ENOENT) is
 * SERVICE_GRAPH_STATE_ABSENT.
 *
 * The edge of a service to its logger, the vertex named <name>SS_LOG_SUFFIX
 * it depends on, carries SERVICE_GRAPH_EDGE_LOGGER besides what posted it.
 * The rest is service_graph_derive().
 *
 * @param[out] g       Graph to build. Must be SERVICE_GRAPH_ZERO or released.
 * @param[in]  base    Base directory of the system.
 * @param[in]  overlay Resolve files a parse holds in memory, or 0.
 *
 * @return 1 when @g holds the graph.
 * @return -1 on a system error, errno set, @g left empty; EOVERFLOW when the
 *         system holds more than SS_MAX_SERVICE names (the parse refuses to go
 *         beyond); the opendir errno when <base>system/.resolve/service cannot
 *         be listed, ENOENT when it is absent.
 *
 * @see service_graph_derive
 */
extern int service_graph_build(service_graph_t *g, char const *base, hash_t *overlay) ;

/**
 * @brief Derive in @g every block computed from its vertexes and depends edges.
 *
 * From its vertexes (name, ndepends, state) and its depends edges with their
 * origins, it derives all the rest: the position of the edges of each vertex,
 * requiredby and rorigin, the exact transpose of depends and dorigin, each
 * slice sorted by index; then the start order (Kahn) and the state
 * SERVICE_GRAPH_STATE_UNORDERED, a vertex read UNORDERED being taken for
 * SERVICE_GRAPH_STATE_OK first. The one derivation of the build and of the
 * cache.
 *
 * Kahn is written here rather than taken from graph_sort() of oblibs, which
 * refuses the whole graph on a cycle: it orders what it can and marks the
 * rest UNORDERED; a vertex left out that does not read closes the order, by
 * index.
 *
 * @param[in,out] g Graph to complete. It holds at most SS_MAX_SERVICE
 *                  vertexes, each name inside strings, each edge toward a
 *                  vertex of @g and as many edges as the ndepends add up to.
 *
 * @return 1 when @g holds the whole graph.
 * @return -1 on a system error, errno set.
 */
extern int service_graph_derive(service_graph_t *g) ;

/**
 *
 * The cache of the graph, one file per owner: <live>graph/<uid>/system, @live
 * given by the caller as the base is, SS_LIVE for a command, a directory of
 * its own for a test; it ends with a slash, as SS_LIVE does.
 *
 * <live>graph is made by root, mode 1777, with root's own directory 0 made in
 * it before it is opened to the others; an owner makes its directory <uid>
 * there, mode 0755. The file is a cdb of oblibs: under one key each, the
 * format and the identity of the base it was built from (st_dev and st_ino of
 * <base>), and the blocks of service_graph_t that service_graph_derive() does
 * not compute, as they are in memory: vertex, depends, dorigin, strings. It
 * lives on the tmpfs of /run: a reboot takes it away.
 *
 * Only a process of the owner writes it, and only under the lock of the system:
 * at the end of a write transaction, or a reader that took the lock with
 * service_graph_write_try() before it built.
 *
 * */

#define SERVICE_GRAPH_CACHE_FORMAT 2 // bumped at any change of what a block holds

// the keys of the cdb: the format, then one per block kept, in the order of the struct
#define SERVICE_GRAPH_CACHE_KEY_FORMAT "format"
#define SERVICE_GRAPH_CACHE_KEYS { "vertex", "depends", "dorigin", "strings" }
#define SERVICE_GRAPH_CACHE_BLOCKS(g) { &(g)->vertex, &(g)->depends, &(g)->dorigin, &(g)->strings }
#define SERVICE_GRAPH_CACHE_NBLOCK 4

// the value of the key format: SERVICE_GRAPH_CACHE_FORMAT (uint32_t), st_dev and st_ino of <base> (uint64_t each)
#define SERVICE_GRAPH_CACHE_FORMAT_LEN (sizeof(uint32_t) + 2 * sizeof(uint64_t))

// write in @dst, SERVICE_GRAPH_CACHE_FORMAT_LEN bytes, the value of the key format for the base of stat @base
static inline void service_graph_cache_format(char *dst, struct stat const *base)
{
    uint32_t format = SERVICE_GRAPH_CACHE_FORMAT ;
    uint64_t dev = (uint64_t)base->st_dev, ino = (uint64_t)base->st_ino ;

    memcpy(dst, &format, sizeof(format)) ;
    memcpy(dst + sizeof(format), &dev, sizeof(dev)) ;
    memcpy(dst + sizeof(format) + sizeof(dev), &ino, sizeof(ino)) ;
}

// <live>graph/<uid> of @owner in @dst, which holds SERVICE_GRAPH_CACHE_DIR_LEN(strlen(@live)) + 1 bytes
#define SERVICE_GRAPH_CACHE_DIR_LEN(livelen) ((livelen) + SS_LIVEGRAPH_LEN + 1 + UID_FMT)
#define SERVICE_GRAPH_CACHE_DIR(dst, live, owner) \
    (auto_strings((dst), (live), SS_LIVEGRAPH "/"), \
    (dst)[strlen(dst) + uid_format((dst) + strlen(dst), (owner))] = 0)

/**
 * @brief Open the directory <uid> of the cache of @owner, checked on its descriptor.
 *
 * The directory must be a directory, not a link, of @owner, that no other
 * account can write. The one check of service_graph_cache_read(),
 * service_graph_cache_write() and service_graph_cache_drop().
 *
 * @param[in] dir   Path of the directory <uid>.
 * @param[in] owner Owner the directory must belong to.
 *
 * @return The descriptor, opened `O_CLOEXEC`, on success.
 * @return -1 with errno EPERM when the directory is refused, ENOENT when it is
 *         absent, else the errno of the call that failed.
 */
static inline int service_graph_cache_dir(char const *dir, uid_t owner)
{
    struct stat st ;
    // O_NOFOLLOW: a link in place of the directory is refused, not followed
    int fd = (int)io_open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) ;

    if (fd < 0)
        return (errno == ELOOP || errno == ENOTDIR) ? (errno = EPERM, -1) : -1 ;

    if (fstat(fd, &st) < 0) {
        close_fd(fd) ;
        return -1 ;
    }

    if (!S_ISDIR(st.st_mode) || st.st_uid != owner || (st.st_mode & (S_IWGRP | S_IWOTH))) {
        close_fd(fd) ;
        return (errno = EPERM, -1) ;
    }

    return fd ;
}

/**
 * @brief Read the cache of @owner, built from the system at @base, into @g.
 *
 * The directory <uid> must be a directory of @owner that no other account can
 * write, the file a regular file, both checked on their descriptor before
 * anything is read; the format must be SERVICE_GRAPH_CACHE_FORMAT and the base
 * the directory @base is now. Each block is copied in @g, then checked for
 * what a reader would read out of bounds, the cache being written by 66 alone:
 * every length a multiple of its element, an origin per edge; at most
 * SS_MAX_SERVICE vertexes; each name inside the strings, zero ended; as many
 * edges as the ndepends add up to, each toward a vertex of the graph. The rest
 * is computed again by service_graph_derive().
 *
 * Whatever keeps the cache from being read counts as no cache: absent,
 * refused, or an open, a stat or a mapping that fails. The caller builds.
 *
 * @param[out] g     Graph to fill. Must be SERVICE_GRAPH_ZERO.
 * @param[in]  owner Owner of the cache.
 * @param[in]  base  Base directory the cache must have been built from.
 * @param[in]  live  Runtime directory holding the cache.
 *
 * @return 1 when @g holds the cached graph.
 * @return 0 when there is no usable cache, @g left empty, errno set: ENOENT
 *         when absent, EINVAL when refused (the directory <uid>, a file that
 *         is not regular, a format, a base, a missing block or a bound), ELOOP
 *         when the file is a link (opened with `O_NOFOLLOW`), else the errno of
 *         the stat of @base, of the open, of the fstat or of the mapping that
 *         failed.
 * @return -1 when the blocks of @g cannot be allocated, by the copy or by
 *         service_graph_derive(), errno set, @g left empty.
 */
extern int service_graph_cache_read(service_graph_t *g, uid_t owner, char const *base, char const *live) ;

/**
 * @brief Write @g, built from the system at @base, as the cache of @owner.
 *
 * The cache is replaced at once: the cdb is made in the temporary directory of
 * set_livetmp() for @live, then renamed in place, a reader sees the old cache
 * or the new one. The directories are made when missing: <live>graph by root
 * only, its 0 first, then mode 1777; <uid> by @owner, mode 0755. The caller
 * holds the lock of the system at @base.
 *
 * @param[in] g     Graph to write.
 * @param[in] owner Owner of the cache.
 * @param[in] base  Base directory @g was built from.
 * @param[in] live  Runtime directory holding the cache.
 *
 * @return 1 when written.
 * @return 0 when the cache of @owner cannot be held: the process is not
 *         @owner, <live>graph is missing and the process is not root, or <uid>
 *         is not a directory of @owner that only @owner writes; every reader
 *         builds the graph.
 * @return -1 on a system error, errno set, no cache written; ENOTDIR when
 *         <live>graph exists and is not a directory, else the errno of the
 *         stat of @base, of the creation of the directories, of set_livetmp()
 *         or of the temporary file, its write or its rename.
 */
extern int service_graph_cache_write(service_graph_t const *g, uid_t owner, char const *base, char const *live) ;

/**
 * @brief Remove the cache of @owner under @live.
 *
 * The caller holds the lock of the system. A directory <uid> that is missing,
 * or that is not a directory of @owner only @owner writes, holds no cache of
 * @owner: nothing is removed.
 *
 * @param[in] owner Owner of the cache.
 * @param[in] live  Runtime directory holding the cache.
 *
 * @return 1 when no cache of @owner is left, absent before included.
 * @return -1 on a system error, errno set.
 */
extern int service_graph_cache_drop(uid_t owner, char const *live) ;

/**
 *
 * Reading, every accessor reads @g only
 *
 * */

// the number of vertexes of @g
static inline uint32_t service_graph_nvertex(service_graph_t const *g)
{
    return (uint32_t)(g->vertex.len / sizeof(service_graph_vertex_t)) ;
}

// the number of vertexes in the start order of @g
static inline uint32_t service_graph_nsort(service_graph_t const *g)
{
    return (uint32_t)(g->sort.len / sizeof(uint32_t)) ;
}

// the vertex @index of @g, @index below service_graph_nvertex()
static inline service_graph_vertex_t const *service_graph_vertex(service_graph_t const *g, uint32_t index)
{
    return (service_graph_vertex_t const *)g->vertex.s + index ;
}

// the name of the vertex @index of @g
static inline char const *service_graph_name(service_graph_t const *g, uint32_t index)
{
    return g->strings.s + service_graph_vertex(g, index)->name ;
}

// the state of the vertex @index of @g, SERVICE_GRAPH_STATE_*
static inline uint8_t service_graph_state(service_graph_t const *g, uint32_t index)
{
    return service_graph_vertex(g, index)->state ;
}

// 1 when a vertex in @state has a resolve file that reads: OK, UNORDERED
static inline int service_graph_readable(uint8_t state)
{
    return state == SERVICE_GRAPH_STATE_OK || state == SERVICE_GRAPH_STATE_UNORDERED ;
}

// 1 when @state makes a vertex invalid by itself: UNREADABLE, UNORDERED
static inline int service_graph_invalid(uint8_t state)
{
    return state == SERVICE_GRAPH_STATE_UNREADABLE || state == SERVICE_GRAPH_STATE_UNORDERED ;
}

// the vertex at @pos of the start order, @pos below service_graph_nsort()
static inline uint32_t service_graph_sort(service_graph_t const *g, uint32_t pos)
{
    return ((uint32_t const *)g->sort.s)[pos] ;
}

/**
 * @brief The vertexes @index depends on.
 *
 * service_graph_vertex(@index)->ndepends of them, their origins at the same
 * position in service_graph_dorigin().
 */
static inline uint32_t const *service_graph_depends(service_graph_t const *g, uint32_t index)
{
    return (uint32_t const *)g->depends.s + service_graph_vertex(g, index)->depends ;
}

// the origins of the edges of service_graph_depends(), at the same position
static inline uint8_t const *service_graph_dorigin(service_graph_t const *g, uint32_t index)
{
    return (uint8_t const *)g->dorigin.s + service_graph_vertex(g, index)->depends ;
}

/**
 * @brief The vertexes that depend on @index.
 *
 * service_graph_vertex(@index)->nrequiredby of them, their origins at the same
 * position in service_graph_rorigin().
 */
static inline uint32_t const *service_graph_requiredby(service_graph_t const *g, uint32_t index)
{
    return (uint32_t const *)g->requiredby.s + service_graph_vertex(g, index)->requiredby ;
}

// the origins of the edges of service_graph_requiredby(), at the same position
static inline uint8_t const *service_graph_rorigin(service_graph_t const *g, uint32_t index)
{
    return (uint8_t const *)g->rorigin.s + service_graph_vertex(g, index)->requiredby ;
}

/**
 * @brief Find the vertex named @name.
 *
 * The search is a dichotomy over the names. @name is taken literally: a name
 * a provider answers for is resolved before, see selection_service_names().
 *
 * @param[in] g    Graph to search.
 * @param[in] name Name of the vertex.
 *
 * @return Its index when found.
 * @return SERVICE_GRAPH_NOINDEX when no vertex is named @name.
 */
extern uint32_t service_graph_find(service_graph_t const *g, char const *name) ;

// the vertex named @name when it has a resolve file, readable or not, SERVICE_GRAPH_NOINDEX otherwise
static inline uint32_t service_graph_find_parsed(service_graph_t const *g, char const *name)
{
    uint32_t index = service_graph_find(g, name) ;

    return index != SERVICE_GRAPH_NOINDEX && service_graph_state(g, index) != SERVICE_GRAPH_STATE_ABSENT ? index : SERVICE_GRAPH_NOINDEX ;
}

/**
 * @brief Find the logger of @index.
 *
 * The logger is the vertex named <name of @index>SS_LOG_SUFFIX that @index
 * depends on, the suffix being reserved to the loggers 66 creates; its edge is
 * marked SERVICE_GRAPH_EDGE_LOGGER. A service on a dependency cycle keeps its
 * logger, which follows it to remove and reconfigure -c (contract, Loggers;
 * Invalid service); a vertex whose resolve file does not read declares no
 * edge, it has none.
 *
 * @param[in] g     Graph to read.
 * @param[in] index Vertex of the service.
 *
 * @return The index of its logger.
 * @return SERVICE_GRAPH_NOINDEX when @index has none.
 */
extern uint32_t service_graph_logger(service_graph_t const *g, uint32_t index) ;

/**
 * @brief Find the service whose logger is @index.
 *
 * @index is a logger when it is a vertex named with SS_LOG_SUFFIX that its
 * service, the vertex named without the suffix, depends on, through the edge
 * marked SERVICE_GRAPH_EDGE_LOGGER.
 *
 * @param[in] g     Graph to read.
 * @param[in] index Vertex to test.
 *
 * @return The index of its service.
 * @return SERVICE_GRAPH_NOINDEX when @index is no logger.
 */
extern uint32_t service_graph_logparent(service_graph_t const *g, uint32_t index) ;

/**
 * @brief Find the module that carries @index.
 *
 * The module is the vertex at the other end of the requiredby edge of @index
 * marked SERVICE_GRAPH_EDGE_CONTENTS.
 *
 * @param[in] g     Graph to read.
 * @param[in] index Vertex of the member.
 *
 * @return The index of its module.
 * @return SERVICE_GRAPH_NOINDEX when no module carries @index.
 */
extern uint32_t service_graph_module(service_graph_t const *g, uint32_t index) ;

/**
 * @brief Tell whether an edge of origin @origin comes from a declared dependency.
 *
 * Whatever else it orders, the edge is declared when a Depends, a RequiredBy,
 * an OptsDepends taken or a logger posted it. SERVICE_GRAPH_EDGE_REQUIREDBY
 * tells, among them, the edges a RequiredBy declared, which 66 remove names.
 *
 * @param[in] origin SERVICE_GRAPH_EDGE_* bits of the edge.
 *
 * @return 1 when it does.
 * @return 0 when only the From of a reactor or the contents of a module posted
 *         it.
 */
static inline int service_graph_origin_isdeclared(uint8_t origin)
{
    return (origin & SERVICE_GRAPH_EDGE_DECLARED) ? 1 : 0 ;
}

/**
 *
 * Invalid vertexes
 *
 * */

/**
 * @brief Tell whether the frontend of @index is gone.
 *
 * Gone means absent from every directory the parse looks in for the caller.
 * It is never kept in the graph: it is searched in @frontends, the listing of
 * those directories, empty until the first search of the command lists them
 * (service_frontend_walk()). A logger and a member of a module have no
 * frontend of their own and are never gone.
 *
 * @param[in]     g         Graph to read.
 * @param[in]     index     Vertex to test.
 * @param[in,out] frontends Listing of the frontend directories, filled by the
 *                          first search of the command and reused after.
 *
 * @return 1 when it is gone.
 * @return 0 when it is found, or when @index has no frontend of its own.
 * @return -1 on a system error, errno set.
 */
extern int service_graph_frontend_gone(service_graph_t const *g, uint32_t index, strbuf *frontends) ;

/**
 * @brief Compose in @sa the line that names an invalid vertex.
 *
 * The line names the vertex, its cause and its repair, as the contract writes
 * it: the refusal of a command and the warning about an invalid vertex
 * elsewhere both say it through this function. A vertex
 * SERVICE_GRAPH_STATE_UNREADABLE names the vertexes that depend on it.
 *
 * @frontend_gone makes a SERVICE_GRAPH_STATE_OK vertex invalid ("has no
 * frontend anywhere any more"), and a SERVICE_GRAPH_STATE_ABSENT vertex one to
 * report ("has neither a resolve file nor a frontend"), the two lines of the
 * contract this function composes beside the invalid states.
 *
 * @param[out] sa            Receives the line, which replaces what it held.
 * @param[in]  g             Graph to read.
 * @param[in]  index         Vertex to describe.
 * @param[in]  frontend_gone The answer of service_graph_frontend_gone() for
 *                           @index.
 *
 * @return 1 when @index is invalid, or absent without frontend, and @sa holds
 *         the line, which replaces what @sa held.
 * @return 0 when @index is valid, nothing written in @sa.
 * @return -1 on a system error, errno set; EINVAL when the state of @index is
 *         beyond SERVICE_GRAPH_STATE_MAX.
 */
extern int service_graph_invalid_message(strbuf *sa, service_graph_t const *g, uint32_t index, bool frontend_gone) ;

/**
 * @brief Append to @sa the names of the vertexes that depend on @index.
 *
 * The names follow the order of their index, separated by a space: what the
 * messages of the contract name after "required by:". Nothing is appended when
 * none depends on @index. @sa is NUL-terminated past its length either way.
 *
 * @param[in,out] sa    Buffer the names are appended to.
 * @param[in]     g     Graph to read.
 * @param[in]     index Vertex whose dependents are named.
 *
 * @return 1 on success.
 * @return -1 on a system error, errno set.
 */
extern int service_graph_dependents(strbuf *sa, service_graph_t const *g, uint32_t index) ;

/**
 * @brief Warn about every invalid vertex of @g outside @skip.
 *
 * Warns, at verbosity 2, about every vertex of @g outside @skip that the graph
 * marks invalid, and every vertex SERVICE_GRAPH_STATE_ABSENT that has no
 * frontend, with the line of service_graph_invalid_message(): a command whose
 * action does not touch an invalid service acts and warns (contract, Invalid
 * service). The frontend of a service that reads is not looked at here: a
 * frontend gone is seen only by a command whose action touches the service
 * (command_refuse_selection()); the absent vertexes, a handful of declared
 * names, are looked at.
 *
 * @param[in]     g         Graph to read.
 * @param[in,out] frontends Listing of the frontend directories, as for
 *                          service_graph_frontend_gone().
 * @param[in]     skip      The members a command refuses on its own, or 0.
 *
 * @return 1 on success.
 * @return -1 on a system error, errno set, the warnings given so far kept.
 */
extern int service_graph_warn_invalid(service_graph_t const *g, strbuf *frontends, bitset32_t const *skip) ;

/**
 *
 * The resolve files of a graph: the services a command reads, a slot per
 * vertex. Unlike the rest of this file, it reads the disk.
 *
 * */

/**
 * @struct service_graph_resolve_s
 * @brief The services a command reads, one slot per vertex of the graph it acts upon.
 *
 * Each service is read once by service_read() and kept until
 * service_graph_resolve_free(). The store belongs to the graph it was sized
 * for: a service_graph_reload() changes every index, it is freed and
 * initialised again on the new graph, as every selection is drawn again.
 *
 * @param g
 * The graph, borrowed: its vertexes are the slots, its names the files read.
 *
 * @param base
 * Where the resolve files are read, borrowed.
 *
 * @param slot
 * One per vertex of @g, 0 until the vertex is read.
 *
 * @param unreadable
 * The member service_graph_resolve_load() found unreadable, its selection not
 * naming it, while @g says it reads, see service_graph_resolve_load();
 * SERVICE_GRAPH_NOINDEX otherwise.
 */
typedef struct service_graph_resolve_s service_graph_resolve_t, *service_graph_resolve_t_ref ;
struct service_graph_resolve_s
{
    service_graph_t const *g ;
    char const *base ;
    struct resolve_hash_s **slot ;
    uint32_t unreadable ;
} ;

#define SERVICE_GRAPH_RESOLVE_ZERO { .g = 0, .base = 0, .slot = 0, .unreadable = SERVICE_GRAPH_NOINDEX }

/**
 * @brief Size @store for @g, whose services are read in @base.
 *
 * One empty slot per vertex, none when @g has no vertex. store->unreadable is
 * SERVICE_GRAPH_NOINDEX.
 *
 * @param[out] store Store to size.
 * @param[in]  g     Graph the store belongs to, borrowed.
 * @param[in]  base  Base directory of the resolve files, borrowed.
 *
 * @return 1 on success.
 * @return -1 when the slots cannot be allocated, errno ENOMEM.
 */
extern int service_graph_resolve_init(service_graph_resolve_t *store, service_graph_t const *g, char const *base) ;

/**
 * @brief Read the service of the vertex @index into @store.
 *
 * The service is read with @addons on the first call for that vertex, or
 * completed with the addons not read yet. @index has a resolve file that
 * reads: SERVICE_GRAPH_STATE_OK or UNORDERED.
 *
 * A vertex the graph says readable whose files no longer read means the graph
 * is stale: something changed the disk behind it. The call then returns 0, and
 * the command loads the graph again (service_graph_reload()) and draws its
 * selections and its store again, once. This 0, and the one of
 * service_graph_resolve_load(), are the only places a stale graph is reported:
 * the steps that read the store after service_graph_resolve_load() find their
 * services there and never read the disk.
 *
 * @param[in,out] store  Store of the graph.
 * @param[in]     index  Vertex to read.
 * @param[in]     addons SERVICE_READ_* addons to read.
 * @param[out]    h      Receives the service; *@h is 0 on every return but 1.
 *
 * @return 1 when @h points to the service.
 * @return 0 when the graph is stale (service_read() gave 0): errno ENOENT when
 *         the resolve file is gone, EINVAL when it, or an addon of @addons it
 *         declares, does not read.
 * @return -1 on a system error, errno set; EINVAL for an index that is no
 *         readable vertex or for a bit of @addons outside SERVICE_READ_ALL, a
 *         fault of the caller.
 */
extern int service_graph_resolve_read(service_graph_resolve_t *store, uint32_t index, uint32_t addons, struct resolve_hash_s **h) ;

/**
 * @brief Read into @store every member of @sel that has a resolve file that reads.
 *
 * service_graph_resolve_read() for each of them: a command reads what it acts
 * upon, with the union of the addons its steps need, before anything moves, so
 * a stale graph is found before the first action. The later steps read the
 * store again, never the disk.
 *
 * The operands are read first: one found stale is drawn again by its name.
 * A member @sel does not name found unreadable (service_read() gave 0, errno
 * EINVAL) is kept in store->unreadable: the command touched it, the new graph
 * no longer links it when its own declarations do not read (contract, Invalid
 * service), and the call returns 0 as for a stale graph. store->unreadable is
 * reset to SERVICE_GRAPH_NOINDEX at each call. The reading stops at the first
 * member that does not give 1.
 *
 * @param[in,out] store  Store of the graph.
 * @param[in]     sel    Selection drawn on the graph of @store.
 * @param[in]     addons Union of the SERVICE_READ_* addons the steps need.
 *
 * @return 1 when every member is read.
 * @return 0 when the graph is stale, see service_graph_resolve_read().
 * @return -1 on a system error, errno set; EINVAL when @sel was not drawn on
 *         the graph of @store.
 */
extern int service_graph_resolve_load(service_graph_resolve_t *store, struct selection_service_s const *sel, uint32_t addons) ;

/**
 * @brief Get from @store the service of the member @index.
 *
 * The service is taken as a step after service_graph_resolve_load() reads it:
 * from @store, never from the disk, with @addons already read. Shared by the
 * steps of lib66/svc, sanitize_init() and the commands.
 *
 * @param[in]  store  Store loaded by service_graph_resolve_load().
 * @param[in]  index  Vertex of the member.
 * @param[in]  addons SERVICE_READ_* addons the caller needs.
 * @param[out] h      Receives the service; *@h is 0 on every return but 1.
 *
 * @return 1 when @h points to it.
 * @return 0 when @index is SERVICE_GRAPH_STATE_UNREADABLE: never loaded, its
 *         slot is empty, *@h is 0, errno EINVAL for a caller that cannot meet
 *         one.
 * @return -1 with errno EINVAL, *@h 0, when @index is beyond the vertexes of
 *         the graph, or when the caller did not load it with @addons (an empty
 *         slot, as for a vertex SERVICE_GRAPH_STATE_ABSENT, included).
 */
extern int service_graph_resolve_get(service_graph_resolve_t *store, uint32_t index, uint32_t addons, struct resolve_hash_s **h) ;

/**
 * @brief Release every service of @store and its slots.
 *
 * store->g is still the graph it was sized for. @store is then
 * SERVICE_GRAPH_RESOLVE_ZERO, a store already SERVICE_GRAPH_RESOLVE_ZERO
 * included.
 */
extern void service_graph_resolve_free(service_graph_resolve_t *store) ;

/**
 *
 * Tree graph, unchanged
 *
 * */

// the flags of tree_graph_resolve() and of the tree_graph_build_* functions
#define GRAPH_WANT_DEPENDS (1 << 3) // follow the Depends of each tree
#define GRAPH_WANT_REQUIREDBY (1 << 4) // follow the RequiredBy of each tree; alone, the sort is reversed
#define GRAPH_WANT_ENABLED (1 << 9) // leave out of the graph a tree that is not enabled

/**
 * @struct tree_graph_s
 * @brief The graph of the trees of a system.
 *
 * @param g
 * The oblibs graph of the names of the trees.
 *
 * @param hres
 * The resolve file of every tree collected, keyed by its name.
 */
typedef struct tree_graph_s tree_graph_t, *tree_graph_t_ref ;
struct tree_graph_s
{
    graph g ;
    hash_t hres ;
} ;

#define GRAPH_TREE_ZERO { GRAPH_ZERO, HASH_ZERO }

// walk the start order of the tree graph @gn of type @type, @pos from its current value, set by the caller, to g.nsort
#define FOREACH_GRAPH_SORT(type,gn,pos) \
    type *__g__##__LINE__ = gn; \
    for (; (uint32_t)pos < __g__##__LINE__->g.nsort; pos++)

/**
 * @brief Add the vertex @name to @g.
 *
 * graph_add_vertex() of oblibs.
 *
 * @param[in,out] g    Graph to add to.
 * @param[in]     name Name of the vertex.
 *
 * @return 1 when @name is a vertex of @g, added or already there.
 * @return 0 when it cannot be added, errno EINVAL whatever the cause (a name
 *         too long, a full graph, an allocation).
 */
extern int graph_add(graph *g, const char *name) ;

/**
 * @brief Append to @stk the names of the neighbours of @v.
 *
 * The names are appended as an sbl: the vertexes @v depends on, or the
 * vertexes that depend on @v when @requiredby is true. Nothing is appended
 * when there is none.
 *
 * @param[in,out] stk        sbl the names are appended to.
 * @param[in]     g          Graph to read.
 * @param[in]     v          Vertex of @g whose edges are read.
 * @param[in]     requiredby false for the Depends edges, true for the
 *                           RequiredBy edges.
 *
 * @return 1 on success.
 * @return 0 when a name cannot be appended, errno ENOMEM.
 */
extern int graph_get_stkedge(strbuf *stk, graph *g, vertex_t *v, bool requiredby) ;

/**
 * @brief Initialise @g for at most @len trees.
 *
 * Initialises its graph, then its hash of resolve files. The graph is released
 * when the hash cannot be made.
 *
 * @param[out] g   Tree graph to initialise.
 * @param[in]  len Maximum number of trees, no more than UINT32_BITS_MAX.
 *
 * @return 1 on success.
 * @return 0 on an allocation failure, errno as the failing allocation left it.
 */
extern int tree_graph_new(tree_graph_t *g, uint32_t len) ;

// release the graph and the resolve files of @g
extern void tree_graph_destroy(tree_graph_t *g) ;

/**
 * @brief Collect the resolve file of @treename and of the trees it names.
 *
 * Adds to g->hres the resolve file of @treename, read from info->base, then,
 * recursively, of every tree its Depends and its RequiredBy name. A tree
 * already in g->hres is not read again, nor walked.
 *
 * The process dies (LOG_EXIT_SYS) when a resolve file cannot be read, a tree
 * cannot be added to g->hres, or a list cannot be cleaned.
 *
 * @param[in,out] g        Tree graph collecting the resolve files.
 * @param[in]     treename Name of the tree.
 * @param[in]     info     Context of the command, info->base read.
 *
 * @return The number of trees added.
 * @return 0 when @treename was already collected.
 */
extern uint32_t tree_graph_collect(tree_graph_t *g, const char *treename, ssexec_t *info) ;

/**
 * @brief Collect every tree of @list.
 *
 * tree_graph_collect() for every name of @list.
 *
 * @param[in,out] g    Tree graph collecting the resolve files.
 * @param[in]     list Names, each ending with a zero.
 * @param[in]     len  Number of bytes of @list.
 * @param[in]     info Context of the command, info->base read.
 *
 * @return The number of trees added.
 * @return 0 when there was none to add.
 */
extern uint32_t tree_graph_ncollect(tree_graph_t *g, const char *list, size_t len, ssexec_t *info) ;

/**
 * @brief Add the tree @treename and its neighbours to g->g, then sort it.
 *
 * Adds to g->g the vertex @treename, collected before, and, as @flag asks, its
 * Depends (GRAPH_WANT_DEPENDS) and its RequiredBy (GRAPH_WANT_REQUIREDBY) with
 * their edges, recursively, the vertexes at the other end of an edge added
 * with it; with GRAPH_WANT_ENABLED, a tree that is not enabled is neither
 * added by its own visit nor walked, but stays a vertex when the edge of a
 * tree walked names it. Then sorts g->g, reversed when @flag holds
 * GRAPH_WANT_REQUIREDBY without GRAPH_WANT_DEPENDS. Each failure is warned at
 * verbosity 2.
 *
 * @param[in,out] g        Tree graph, @treename collected in g->hres.
 * @param[in]     treename Name of the tree.
 * @param[in]     flag     GRAPH_WANT_DEPENDS, GRAPH_WANT_REQUIREDBY,
 *                         GRAPH_WANT_ENABLED.
 *
 * @return 1 on success.
 * @return 0 on failure: @treename or a tree it names not collected, a vertex
 *         or an edge that cannot be added, a list that cannot be cleaned, a
 *         sort that fails (a cycle); errno EINVAL when a vertex cannot be
 *         added, else as the failing call left it, untouched when a tree is
 *         not collected.
 */
extern int tree_graph_resolve(tree_graph_t *g, const char *treename, uint32_t flag) ;

/**
 * @brief Add every tree of @list to g->g, then sort it once.
 *
 * tree_graph_resolve() for every name of @list, the graph sorted once at the
 * end.
 *
 * @param[in,out] g    Tree graph, the trees of @list collected in g->hres.
 * @param[in]     list Names, each ending with a zero.
 * @param[in]     len  Number of bytes of @list.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return 1 on success.
 * @return 0 on failure, errno as for tree_graph_resolve().
 */
extern int tree_graph_nresolve(tree_graph_t *g, const char *list, size_t len, uint32_t flag) ;

/**
 * @brief Build the tree graph of the trees of @list.
 *
 * tree_graph_ncollect() then tree_graph_nresolve() on @list.
 *
 * @param[in,out] g    Tree graph to build.
 * @param[in]     list Names, each ending with a zero.
 * @param[in]     len  Number of bytes of @list.
 * @param[in]     info Context of the command, info->base read.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return The number of trees collected.
 * @return 0 when none was collected, errno untouched; or when the resolution
 *         fails, errno EINVAL.
 */
extern uint32_t tree_graph_build_list(tree_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Build the tree graph of every tree of info->base.
 *
 * tree_graph_build_list() on the regular files of <base>system/.resolve but
 * Master.
 *
 * @param[in,out] g    Tree graph to build.
 * @param[in]     info Context of the command, info->base read.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return As tree_graph_build_list().
 * @return 0 as well when the directory cannot be listed, warned at verbosity
 *         2, errno of sbl_dir_get().
 */
extern uint32_t tree_graph_build_system(tree_graph_t *g, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Build the tree graph of the tree @name.
 *
 * tree_graph_collect() then tree_graph_resolve() on the tree @name.
 *
 * @param[in,out] g    Tree graph to build.
 * @param[in]     name Name of the tree.
 * @param[in]     info Context of the command, info->base read.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return The number of trees collected.
 * @return 0 when none was collected, errno untouched; or when the resolution
 *         fails, errno EINVAL.
 */
extern uint32_t tree_graph_build_name(tree_graph_t *g, const char *name, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Build the tree graph of the contents of the Master resolve file.
 *
 * tree_graph_build_list() on the contents of the Master resolve file of
 * info->base. The process dies (LOG_EXIT_SYS) when that field cannot be read.
 *
 * @param[in,out] g    Tree graph to build.
 * @param[in]     info Context of the command, info->base read.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return As tree_graph_build_list().
 */
extern uint32_t tree_graph_build_master(tree_graph_t *g, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Build the tree graph of the trees of the group info->treename.
 *
 * tree_graph_build_list() on the trees of the Master contents of info->base
 * that belong to the group info->treename (tree_ongroups()). The process dies
 * (LOG_EXIT_SYS) when the Master field cannot be read, or on an allocation
 * failure of the selection.
 *
 * @param[in,out] g    Tree graph to build.
 * @param[in]     info Context of the command, info->base and info->treename
 *                     read.
 * @param[in]     flag As for tree_graph_resolve().
 *
 * @return The number of trees collected.
 * @return 0 with errno 0 when no tree is in the group.
 * @return 0 with errno EINVAL when the groups of a tree cannot be read, or the
 *         resolution fails.
 * @return 0 with errno untouched when none was collected.
 */
extern uint32_t tree_graph_build_groups(tree_graph_t *g, ssexec_t *info, uint32_t flag) ;

/**
 *
 * Writing the system
 *
 * The system of an owner is what its parse and its administration write under
 * <base>/system: the resolve files and addons, the service directories, the
 * trees, the Provide links, the version file. A command writes it inside a
 * write transaction, and only there. The state and status records of the
 * supervision are not part of it: they are written while the command acts on
 * the supervision, outside any transaction (contract, Concurrent commands).
 *
 *
 * - service_graph_write_begin() takes the exclusive lock of the system, a flock on
 *   the directory <base> itself, and drops the cache of the graph of the owner
 *   before the first write;
 * - service_graph_write_end() builds the cache again from the disk and frees the lock,
 *   and gives the graph it built to a command that goes on with it.
 *
 * Whatever kills the command in between, the cache is absent, never stale: the
 * next reader builds the graph from the disk. A command holds the lock while it
 * writes on disk, never while it acts on the supervision: a script of a
 * service may call 66 itself.
 *
 * A second writer waits for the first one, and says so at verbosity 3 once the
 * wait lasts more than a second. A reader never waits: it takes the lock only
 * when it is free at once (service_graph_write_try()), BEFORE it builds the graph, to
 * write the cache of what it built under the lock.
 *
 * The transaction is a state of the process, not of a structure: its owner,
 * base and live directory, where the cache of the graph lives (SS_LIVE for a
 * command, see service_graph_cache_write()), are copied in at its first begin,
 * nothing of the caller is kept. A
 * process holds one transaction at a time, nested begins included.
 *
 * */

/**
 * @brief Open a write transaction on the system of @owner at @base.
 *
 * Opens the transaction, its cache under @live, or nests in the one the
 * process already holds on the same system. The first begin waits for the
 * lock, creating <base> when missing, empties <base>/.tmp of what a killed
 * writer left there, and drops the cache. A nested begin drops the cache as
 * well, whatever the level it nests in. A cache that cannot be dropped fails
 * the begin: nothing more is held, nothing written.
 *
 * @param[in] owner Owner of the system.
 * @param[in] base  Base directory of the system, no longer than
 *                  SS_MAX_PATH_LEN.
 * @param[in] live  Runtime directory holding the cache, no longer than
 *                  SS_MAX_PATH_LEN.
 *
 * @return 1 when the transaction is held.
 * @return -1 on a system error, errno set, nothing held; EDEADLK when the
 *         process already holds a transaction on another system (another
 *         @owner, @base or @live); ENAMETOOLONG when @base or @live is longer
 *         than SS_MAX_PATH_LEN; else the errno of the open or creation of
 *         <base>, of the flock, of the drop of the cache or of the emptying of
 *         <base>/.tmp.
 *
 * @see service_graph_write_end
 */
extern int service_graph_write_begin(uid_t owner, char const *base, char const *live) ;

/**
 * @brief Take the lock of the system of @owner at @base only if it is free at once.
 *
 * For a reader that found no cache under @live: it takes the lock BEFORE it
 * builds the graph, builds, writes the cache, then ends the hold. Without the
 * lock, it builds and never writes: a graph built while a writer holds the
 * lock may be stale. Nested in a transaction of the process on the same
 * system, it only nests. The hold does not drop the cache and writes nothing
 * but the cache.
 *
 * @param[in] owner Owner of the system.
 * @param[in] base  Base directory of the system.
 * @param[in] live  Runtime directory holding the cache.
 *
 * @return 1 when the lock is held.
 * @return 0 when another process holds it, or when <base> does not exist:
 *         nothing to protect, nothing created.
 * @return -1 on a system error, errno set; EDEADLK and ENAMETOOLONG as for
 *         service_graph_write_begin(), else the errno of the open of <base> or
 *         of the flock.
 *
 * @see service_graph_write_end
 */
extern int service_graph_write_try(uid_t owner, char const *base, char const *live) ;

/**
 * @brief Close one level of the transaction, or the hold of service_graph_write_try().
 *
 * The last level frees the lock whatever the build gives: a failed build
 * leaves the cache absent.
 *
 * Only the last level writes the cache, built from the disk: the system is
 * written by then. Without @g, it builds the cache alone. With @g, for a
 * command that goes on with the graph of the system as it is written: @g,
 * emptied of the graph it held, is built from the disk at every level and
 * given back, and at the last level that one build is the cache.
 *
 * @param[in,out] g Graph to build again and give back, or 0.
 *
 * @return 1 on success, @g holding the graph when given. An owner that has no
 *         place for its cache (service_graph_cache_write() returns 0) is not
 *         an error.
 * @return 0 when the cache could not be built or written for a system error: a
 *         success all the same, the system written, @g holding the graph when
 *         given, the cache absent; the caller warns, errno set.
 * @return -1 when no transaction is held, errno EINVAL, @g untouched; or when
 *         @g could not be built, errno set, @g left empty, the level closed
 *         and, at the last one, the lock freed.
 */
extern int service_graph_write_end(service_graph_t *g) ;

#endif
