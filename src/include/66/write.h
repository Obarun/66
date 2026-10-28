/*
 * write.h
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

#ifndef SS_WRITE_H
#define SS_WRITE_H

#include <stdint.h>
#include <66/service.h>
extern int write_services(resolve_service_t *res, resolve_service_addon_execute_t *ex, strbuf const *copyfrom, char const *dst) ;
extern int write_logger(resolve_service_addon_execute_t *ex, char const *dst) ;
extern int write_common(resolve_service_addon_execute_t *ex, strbuf const *copyfrom, char const *dst) ;
extern int write_execute_scripts(char const *file, char const *contents, char const *dst, char const *runas) ;
extern int write_environ(char const *name, char const *contents, char const *dst) ;

#endif
