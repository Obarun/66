/*
 * selection.h
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

#ifndef SS_SELECTION_H
#define SS_SELECTION_H

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/bits.h>

#include <66/graph.h>

#define SELECTION_SERVICE_PROPAGATE_NONE 0
#define SELECTION_SERVICE_PROPAGATE_DEPENDS 1
#define SELECTION_SERVICE_PROPAGATE_REQUIREDBY 2

#define SELECTION_SERVICE_REFUSE_NAMED_ABSENT (1 << 0)
#define SELECTION_SERVICE_REFUSE_MET_ABSENT (1 << 1)
#define SELECTION_SERVICE_REFUSE_NAMED_INVALID (1 << 2)
#define SELECTION_SERVICE_REFUSE_MET_INVALID (1 << 3)

typedef struct selection_service_policy_s selection_service_policy_t, *selection_service_policy_t_ref ;
struct selection_service_policy_s
{
    uint8_t propagate ;
    bool from ;
    bitset32_t const *reactors ;
    bool logger ;
} ;

#define SELECTION_SERVICE_POLICY_ZERO { .propagate = SELECTION_SERVICE_PROPAGATE_NONE, .from = false, .reactors = 0, .logger = false }

typedef struct selection_service_s selection_service_t, *selection_service_t_ref ;
struct selection_service_s
{
    service_graph_t const *g ;
    bitset32_t named ;
    bitset32_t member ;
    bitset32_t met ;
} ;

#define SELECTION_SERVICE_ZERO { .g = 0, .named = BITSET32_ZERO, .member = BITSET32_ZERO, .met = BITSET32_ZERO }

#define FOREACH_SELECTION(sel, pos, index) \
    for (pos = 0 ; pos < service_graph_nsort((sel)->g) ; pos++) \
        if (bitset32_isvalid(&(sel)->member, (index) = service_graph_sort((sel)->g, pos)))

#define FOREACH_SELECTION_REVERSE(sel, pos, index) \
    for (pos = service_graph_nsort((sel)->g) ; pos-- ; ) \
        if (bitset32_isvalid(&(sel)->member, (index) = service_graph_sort((sel)->g, pos)))

#define FOREACH_SELECTION_MEMBER(sel, index) \
    for (index = 0 ; index < service_graph_nvertex((sel)->g) ; index++) \
        if (bitset32_isvalid(&(sel)->member, index))

#define FOREACH_SELECTION_UNORDERED(sel, index) \
    for (index = 0 ; index < service_graph_nvertex((sel)->g) ; index++) \
        if (bitset32_isvalid(&(sel)->member, index) && service_graph_state((sel)->g, index) == SERVICE_GRAPH_STATE_UNORDERED)

extern int selection_service_names(bitset32_t *named, service_graph_t const *g, char const *base, char const *const *argv, int argc, bool provide, int *unknown) ;
extern void selection_service_propagate(selection_service_t *sel, service_graph_t const *g, bitset32_t const *seeds, selection_service_policy_t const *policy) ;
extern void selection_service_remove(selection_service_t *sel, uint32_t index) ;
extern int selection_service_carried(selection_service_t const *sel, uint32_t index) ;
extern uint32_t selection_service_edges(selection_service_t const *sel, uint32_t index, uint32_t *list, bool requiredby) ;
extern int selection_svc_filter(selection_service_t *out, selection_service_t const *sel, service_graph_resolve_t *store, uint32_t addons, int (*keep)(struct resolve_hash_s const *h)) ;
extern int selection_svc_supervised(selection_service_t *out, selection_service_t const *sel, service_graph_resolve_t *store) ;

static inline void selection_service_ordered(selection_service_t *out, selection_service_t const *sel)
{
    uint32_t index ;

    *out = *sel ;

    FOREACH_SELECTION_UNORDERED(sel, index)
        selection_service_remove(out, index) ;
}

#endif
