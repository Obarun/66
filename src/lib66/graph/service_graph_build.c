/*
 * service_graph_build.c
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/enum_parser.h>

typedef struct edge_s edge_t ;
struct edge_s
{
    uint32_t from ;
    uint32_t to ;
    uint8_t origin ;
} ;

typedef struct decl_s decl_t ;
struct decl_s
{
    strbuf names ;
    strbuf from ;
    strbuf to ;
    strbuf origin ;
    strbuf ok ;
    strbuf unreadable ;
} ;

#define DECL_ZERO { STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO, STRBUF_ZERO }

static void decl_free(decl_t *d)
{
    strbuf_free(&d->names) ;
    strbuf_free(&d->from) ;
    strbuf_free(&d->to) ;
    strbuf_free(&d->origin) ;
    strbuf_free(&d->ok) ;
    strbuf_free(&d->unreadable) ;
}

static int list_resolves(strbuf *list, char const *base)
{
    char const *exclude[2] = { SS_PROVIDE + 1, 0 } ;
    char dir[strlen(base) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1] ;

    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    return sbl_dir_get_recursive(list, dir, exclude, FLAGS_LNK, 0) ? 1 : -1 ;
}

static int declare_list(decl_t *d, char const *service, char const *list, uint8_t origin, int reverse, bool literal, hash_t *overlay, char const *base)
{
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;
    size_t pos = 0 ;

    if (!*list)
        return 1 ;

    if (!sbl_clean_string(&stk, list))
        return -1 ;

    FOREACH_SBL(&stk, pos) {

        char buf[SS_MAX_SERVICE_NAME + 1] ;
        char const *name = stk.s + pos ;

        int r = literal ? 0 : service_resolve_provide(buf, name, overlay, base) ;
        if (r < 0)
            return -1 ;

        if (r)
            name = buf ;

        if (!sbl_add(&d->names, name) ||
            !sbl_add(&d->from, reverse ? name : service) ||
            !sbl_add(&d->to, reverse ? service : name) ||
            !strbuf_catb(&d->origin, (char const *)&origin, 1))
                return -1 ;
    }

    return 1 ;
}

// a service that reads: its declarations and its state
static int declare(decl_t *d, struct resolve_hash_s const *h, char const *name, hash_t *overlay, char const *base)
{
    resolve_service_addon_dependencies_t const *dep = &h->dependencies ;
    resolve_service_addon_event_t const *ev = &h->event ;
    bool literal = h->res.islog ;

    if (!sbl_add(&d->ok, name))
        return -1 ;

    if (dep->ndepends && declare_list(d, name, dep->sa.s + dep->depends, SERVICE_GRAPH_EDGE_DECLARED, 0, literal, overlay, base) < 0)
        return -1 ;

    if (dep->noptsdeps && declare_list(d, name, dep->sa.s + dep->optsdeps, SERVICE_GRAPH_EDGE_DECLARED, 0, literal, overlay, base) < 0)
        return -1 ;

    if (dep->nrequiredby && declare_list(d, name, dep->sa.s + dep->requiredby, SERVICE_GRAPH_EDGE_DECLARED|SERVICE_GRAPH_EDGE_REQUIREDBY, 1, literal, overlay, base) < 0)
        return -1 ;

    if (h->res.type == E_PARSER_TYPE_MODULE && dep->ncontents && declare_list(d, name, dep->sa.s + dep->contents, SERVICE_GRAPH_EDGE_CONTENTS, 0, literal, overlay, base) < 0)
        return -1 ;

    if (ev->nfrom && declare_list(d, name, ev->sa.s + ev->from, SERVICE_GRAPH_EDGE_FROM, 0, literal, overlay, base) < 0)
        return -1 ;

    return 1 ;
}

static int read_system(decl_t *d, char const *base, hash_t *overlay)
{
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;
    size_t pos = 0 ;

    if (list_resolves(&list, base) < 0)
        return -1 ;

    FOREACH_SBL(&list, pos) {

        char const *name = list.s + pos ;
        struct resolve_hash_s h = RESOLVE_HASH_ZERO ;

        if (!sbl_add(&d->names, name))
            return -1 ;

        if (overlay && resolve_hash_search(overlay, name))
            continue ;

        int r = service_read(&h, base, name, SERVICE_READ_ALL) ;

        if (r > 0) {

            r = declare(d, &h, name, overlay, base) ;

        } else if (!r && errno == EINVAL) {

            r = sbl_add(&d->unreadable, name) ? 1 : -1 ;

        } else if (!r) {

            r = 1 ;
        }
        service_read_free(&h) ;

        if (r < 0)
            return -1 ;
    }

    if (overlay) {

        struct resolve_hash_s *c, *tmp ;

        HASH_FOREACH(overlay, c, tmp) {

            if (!sbl_add(&d->names, c->name) || declare(d, c, c->name, overlay, base) < 0)
                return -1 ;
        }
    }

    return 1 ;
}

static void set_state(service_graph_t *g, strbuf *list, uint8_t state)
{
    size_t pos = 0 ;
    service_graph_vertex_t *vertex = (service_graph_vertex_t *)g->vertex.s ;

    FOREACH_SBL(list, pos)
        vertex[service_graph_find(g, list->s + pos)].state = state ;
}

static int set_vertexes(service_graph_t *g, decl_t *d)
{
    size_t pos = 0 ;

    if (!d->names.len)
        return 1 ;

    if (!sbl_sortndrop(&d->names))
        return -1 ;

    if (sbl_count(&d->names) > SS_MAX_SERVICE)
        return (errno = EOVERFLOW, -1) ;

    if (!strbuf_reserve(&g->vertex, sbl_count(&d->names) * sizeof(service_graph_vertex_t)) ||
        !strbuf_reserve(&g->strings, d->names.len))
            return -1 ;

    FOREACH_SBL(&d->names, pos) {

        service_graph_vertex_t v = SERVICE_GRAPH_VERTEX_ZERO ;
        char const *name = d->names.s + pos ;

        v.state = SERVICE_GRAPH_STATE_ABSENT ;
        v.name = (uint32_t)g->strings.len ;

        if (!strbuf_catb(&g->strings, name, strlen(name) + 1) ||
            !strbuf_catb(&g->vertex, (char const *)&v, sizeof(v)))
                return -1 ;
    }

    set_state(g, &d->ok, SERVICE_GRAPH_STATE_OK) ;
    set_state(g, &d->unreadable, SERVICE_GRAPH_STATE_UNREADABLE) ;

    return 1 ;
}

static int cmp_depends(void const *a, void const *b)
{
    edge_t const *x = a, *y = b ;

    if (x->from != y->from)
        return x->from < y->from ? -1 : 1 ;

    return x->to < y->to ? -1 : x->to > y->to ;
}

static int is_logger(service_graph_t const *g, uint32_t from, uint32_t to)
{
    char const *name = service_graph_name(g, from), *dname = service_graph_name(g, to) ;
    size_t len = strlen(name) ;

    return get_rstrlen_until(dname, SS_LOG_SUFFIX) == (ssize_t)len && !strncmp(dname, name, len) ;
}

static int collect_edges(strbuf *edges, service_graph_t const *g, decl_t *d)
{
    size_t pf = 0, pt = 0, n = d->origin.len, kept = 0 ;
    edge_t *e ;

    if (!n)
        return 1 ;

    if (!strbuf_reserve(edges, n * sizeof(edge_t)))
        return -1 ;

    e = (edge_t *)edges->s ;

    for (size_t i = 0 ; i < n ; i++) {

        e[i].from = service_graph_find(g, d->from.s + pf) ;
        e[i].to = service_graph_find(g, d->to.s + pt) ;
        e[i].origin = (uint8_t)d->origin.s[i] ;
        pf += strlen(d->from.s + pf) + 1 ;
        pt += strlen(d->to.s + pt) + 1 ;
    }

    qsort(e, n, sizeof(edge_t), cmp_depends) ;

    for (size_t i = 1 ; i < n ; i++) {

        if (e[i].from == e[kept].from && e[i].to == e[kept].to)
            e[kept].origin |= e[i].origin ;
        else
            e[++kept] = e[i] ;
    }

    edges->len = (kept + 1) * sizeof(edge_t) ;

    for (size_t i = 0 ; i <= kept ; i++)
        if (is_logger(g, e[i].from, e[i].to))
            e[i].origin |= SERVICE_GRAPH_EDGE_LOGGER ;

    return 1 ;
}

static int set_depends(service_graph_t *g, edge_t const *e, uint32_t n)
{
    service_graph_vertex_t *vertex = (service_graph_vertex_t *)g->vertex.s ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        if (!strbuf_catb(&g->depends, (char const *)&e[pos].to, sizeof(e[pos].to)) ||
            !strbuf_catb(&g->dorigin, (char const *)&e[pos].origin, 1))
                return -1 ;

        vertex[e[pos].from].ndepends++ ;
    }

    return 1 ;
}

static int transpose(service_graph_t *g)
{
    service_graph_vertex_t *vertex = (service_graph_vertex_t *)g->vertex.s ;
    uint32_t n = service_graph_nvertex(g), nedge = (uint32_t)(g->depends.len / sizeof(uint32_t)), pos = 0 ;
    uint32_t const *depends = (uint32_t const *)g->depends.s ;
    uint8_t const *dorigin = (uint8_t const *)g->dorigin.s ;
    uint32_t *rby, next[n + 1] ;
    uint8_t *rorigin ;

    for (uint32_t index = 0 ; index < n ; index++) {
        vertex[index].depends = pos ;
        vertex[index].nrequiredby = 0 ;
        pos += vertex[index].ndepends ;
    }

    for (uint32_t k = 0 ; k < nedge ; k++)
        vertex[depends[k]].nrequiredby++ ;

    pos = 0 ;

    for (uint32_t index = 0 ; index < n ; index++) {
        vertex[index].requiredby = next[index] = pos ;
        pos += vertex[index].nrequiredby ;
    }

    if (!strbuf_reserve(&g->requiredby, nedge * sizeof(uint32_t)) || !strbuf_reserve(&g->rorigin, nedge))
        return -1 ;

    rby = (uint32_t *)g->requiredby.s ;
    rorigin = (uint8_t *)g->rorigin.s ;

    for (uint32_t index = 0 ; index < n ; index++) {

        for (uint32_t k = vertex[index].depends ; k < vertex[index].depends + vertex[index].ndepends ; k++) {

            rby[next[depends[k]]] = index ;
            rorigin[next[depends[k]]++] = dorigin[k] ;
        }
    }

    g->requiredby.len = nedge * sizeof(uint32_t) ;
    g->rorigin.len = nedge ;

    return 1 ;
}

/* Kahn: a vertex is emitted once every vertex it depends on is, so a cycle and
 * what depends on it never enter the queue while the rest is ordered.
 * graph_sort() of oblibs refuses the whole graph on a cycle. A vertex left out
 * is UNORDERED, unless it has no resolve file that reads: its state names the
 * first cause (contract, Invalid service), and it closes the start order, by
 * index, so that the order holds every vertex that is not UNORDERED. */
static int sort(service_graph_t *g)
{
    service_graph_vertex_t *vertex = (service_graph_vertex_t *)g->vertex.s ;
    uint32_t n = service_graph_nvertex(g), head = 0, tail = 0 ;
    uint32_t pending[n + 1], queue[n + 1] ;
    uint8_t emitted[n + 1] ;

    memset(emitted, 0, sizeof(emitted)) ;

    for (uint32_t index = 0 ; index < n ; index++) {

        pending[index] = vertex[index].ndepends ;

        if (!pending[index])
            queue[tail++] = index ;
    }

    while (head < tail) {

        uint32_t index = queue[head++] ;
        uint32_t const *rby = service_graph_requiredby(g, index) ;

        emitted[index] = 1 ;

        if (!strbuf_catb(&g->sort, (char const *)&index, sizeof(index)))
            return -1 ;

        for (uint32_t pos = 0 ; pos < vertex[index].nrequiredby ; pos++) {

            if (!--pending[rby[pos]])
                queue[tail++] = rby[pos] ;
        }
    }

    for (uint32_t index = 0 ; index < n ; index++) {

        if (emitted[index])
            continue ;

        if (vertex[index].state == SERVICE_GRAPH_STATE_OK) {

            vertex[index].state = SERVICE_GRAPH_STATE_UNORDERED ;

        } else if (!strbuf_catb(&g->sort, (char const *)&index, sizeof(index)))
            return -1 ;
    }

    return 1 ;
}

int service_graph_derive(service_graph_t *g)
{
    log_flow() ;

    service_graph_vertex_t *vertex = (service_graph_vertex_t *)g->vertex.s ;
    uint32_t index = 0 ;

    for (; index < service_graph_nvertex(g) ; index++)
        if (vertex[index].state == SERVICE_GRAPH_STATE_UNORDERED)
            vertex[index].state = SERVICE_GRAPH_STATE_OK ;

    g->requiredby.len = g->rorigin.len = g->sort.len = 0 ;

    return transpose(g) < 0 ? -1 : sort(g) ;
}

static int fill(service_graph_t *g, decl_t *d)
{
    _cleanup_strbuf_ strbuf edges = STRBUF_ZERO ;

    if (set_vertexes(g, d) < 0 || collect_edges(&edges, g, d) < 0 ||
        set_depends(g, (edge_t const *)edges.s, (uint32_t)(edges.len / sizeof(edge_t))) < 0)
            return -1 ;

    return service_graph_derive(g) ;
}

int service_graph_build(service_graph_t *g, char const *base, hash_t *overlay)
{
    log_flow() ;

    int e = -1 ;
    decl_t d = DECL_ZERO ;

    if (read_system(&d, base, overlay) < 0 || fill(g, &d) < 0)
        goto err ;

    e = 1 ;

    err:
        if (e < 0)
            service_graph_free(g) ;
        decl_free(&d) ;
        return e ;
}
