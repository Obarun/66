/*
 * sanitize.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_SANITIZE_H
#define SS_SANITIZE_H

#include <stdint.h>

#include <66/service.h>
#include <66/ssexec.h>

extern int sanitize_system(ssexec_t *info) ;

extern void sanitize_source(char const *name, ssexec_t *info, uint32_t flag) ;
extern void sanitize_fdholder(resolve_service_t *res, uint32_t flag) ;
extern void sanitize_livestate(resolve_service_t *res, uint32_t flag) ;
extern void sanitize_scandir(resolve_service_t *res, uint32_t flag) ;
extern void sanitize_init(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen, uint32_t flags) ;
extern void sanitize_graph(ssexec_t *info) ;

extern int sanitize_backup(resolve_service_t *res, uint32_t flag) ;

#endif
