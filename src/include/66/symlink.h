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
extern int symlink_type(const char *path) ;
extern int symlink_provide(const char *base, resolve_service_t *res, bool action) ;

/** Create the symbolic link @name pointing to @target, atomically replacing any
 * existing @name. If @name does not exist, symlink() is used directly; otherwise
 * the new link is built under a temporary name in the same directory then
 * rename()d onto @name, so @name appears all-or-nothing and a failure leaves any
 * previous @name untouched.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int symlink_atomic(char const *target, char const *name) ;
#endif
