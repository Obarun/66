/*
 * migrate.h
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_MIGRATE_H
#define SS_MIGRATE_H

#include <stdint.h>

#include <oblibs/cdb.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/migrate_0802.h>
#include <66/migrate_0811.h>
#include <66/migrate_0821.h>

extern void migrate_create_snap(ssexec_t *info, const char *version) ;
extern void migrate_ensure_log_owner(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_service_addon_logger_t *lg) ;
extern void migrate_0802() ;
extern void migrate_0811() ;
extern void migrate_0821() ;
extern void migrate_0822() ;
extern void migrate_0900() ;

extern int migrate_write_frozen_0821(resolve_service_t_0821 *res, char const *base, char const *name) ;
extern int service_resolve_read_cdb_0821(ocdb *c, resolve_service_t_0821 *res) ;

#endif
