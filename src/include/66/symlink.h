/*
 * symlink.h
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_SYMLINK_H
#define SS_SYMLINK_H

#include <stdint.h>
#include <sys/types.h>

#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/graph.h>

extern int symlink_type(const char *path) ;
extern int symlink_provide_parse(char const *base, hash_t *hres) ;
extern int symlink_provide_enable(char const *base, struct resolve_hash_s const *h) ;
extern int symlink_provide_release(char const *base, char const *service) ;
extern int symlink_provide_drop(char const *base, char const *name) ;
extern int symlink_provide_isavailable(char const *base, char const *name, uid_t owner, strbuf *msg) ;
extern int symlink_provide_claim(service_graph_t const *g, char const *base, hash_t *along, char const *service, char const *name, strbuf *msg) ;
extern int symlink_provide_isfree(char const *base, char const *service, char const *name, char const *holder, strbuf *msg) ;
extern int symlink_atomic_at(char const *tmpdir, char const *target, char const *name) ;
extern int symlink_atomic_live(uid_t owner, char const *live, char const *target, char const *name) ;
extern int symlink_atomic(char const *target, char const *name) ;
#endif
