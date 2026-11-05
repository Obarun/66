/*
 * sanitize.h
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_SANITIZE_H
#define SS_SANITIZE_H

#include <stdint.h>

#include <66/fdholder.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/selection.h>

extern int sanitize_system(ssexec_t *info) ;
extern int sanitize_migrate(ssexec_t *info, const char *oversion) ;
extern int sanitize_version(ssexec_t *info) ;
extern int sanitize_fdholder_start(fdholder_client_t *c, const char *socket) ;
extern int sanitize_fdholder(resolve_service_t *res, fdholder_client_t *c, ss_state_t *sta, uint32_t flag, uint8_t init) ;
extern int sanitize_livestate(resolve_service_t *res, ss_state_t *sta) ;
extern int sanitize_scandir(resolve_service_t *res, ss_state_t *sta) ;
extern int sanitize_init(selection_service_t const *lot, service_graph_resolve_t *store, uint8_t stage, uint8_t who) ;
extern int sanitize_resolve(ssexec_t *info, uint8_t type) ;

#endif
