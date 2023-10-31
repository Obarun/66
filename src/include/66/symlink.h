/*
 * symlink.h
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_SYMLINK_H
#define SS_SYMLINK_H

#define SYMLINK_SOURCE 0
#define SYMLINK_LIVE 1

#include <stdint.h>
#include <66/service.h>

extern int symlink_switch(resolve_service_t *res, uint8_t flag) ;
extern int symlink_make(resolve_service_t *res) ;

#endif
