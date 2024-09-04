/*
 * snapshot.h
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_SNAPSHOT_H
#define SS_SNAPSHOT_H

#include <66/config.h>

typedef struct snapshot_list_s snapshot_list_t ;
struct snapshot_list_s
{
    char *name ;
} ;

extern snapshot_list_t snapshot_root_list[] ;

extern snapshot_list_t snapshot_user_list[] ;

#endif
