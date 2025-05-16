/*
 * migrate.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <66/ssexec.h>
#include <66/service.h>
#include <66/migrate_0721.h>
#include <66/migrate_0802.h>
#include <66/migrate_0811.h>

extern void migrate_create_snap(ssexec_t *info, const char *version) ;
extern void migrate_ensure_log_owner(resolve_service_t *res) ;
extern void migrate_0721(ssexec_t *info) ;
extern void migrate_0802() ;
extern void migrate_0811() ;

#endif
