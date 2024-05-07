/*
 * module.h
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

#ifndef SS_MODULE_H
#define SS_MODULE_H

#include <sys/types.h>

#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/info.h>

#define SS_MODULE_CONFIG_DIR "/configure"
#define SS_MODULE_CONFIG_DIR_LEN (sizeof SS_MODULE_CONFIG_DIR - 1)
#define SS_MODULE_CONFIG_SCRIPT "configure"
#define SS_MODULE_CONFIG_SCRIPT_LEN (sizeof SS_MODULE_CONFIG_SCRIPT - 1)
#define SS_MODULE_FRONTEND "/frontend"
#define SS_MODULE_FRONTEND_LEN (sizeof SS_MODULE_FRONTEND - 1)
#define SS_MODULE_ACTIVATED "/activated"
#define SS_MODULE_ACTIVATED_LEN (sizeof SS_MODULE_ACTIVATED - 1)
#define SS_MODULE_DEPENDS "/depends"
#define SS_MODULE_DEPENDS_LEN (sizeof SS_MODULE_DEPENDS - 1)
#define SS_MODULE_REQUIREDBY "/requiredby"
#define SS_MODULE_REQUIREDBY_LEN (sizeof SS_MODULE_REQUIREDBY - 1)

extern void parse_module(resolve_service_t *res, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force) ;
extern void parse_module_check_dir(char const *src,char const *dir) ;
extern void parse_module_check_name(char const *src, char const *name) ;

extern void regex_configure(resolve_service_t *res, ssexec_t *info, char const *path, char const *name) ;
extern void regex_rename(stralloc *list, resolve_service_t *res, uint32_t element) ;
extern void regex_replace(stralloc *list, resolve_service_t *res) ;
extern uint8_t regex_get_file_name(stack *filename, char const *str) ;

extern void get_list(stralloc *list, char const *src, char const *name, mode_t mode, char const **exclude) ;
#endif
