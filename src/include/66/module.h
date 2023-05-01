/*
 * module.h
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

#ifndef SS_MODULE_H
#define SS_MODULE_H

#include <sys/types.h>

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

extern void get_list(stralloc *list, char const *src, char const *name, mode_t mode) ;
extern void parse_module(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force) ;
extern void parse_module_check_dir(char const *src,char const *dir) ;
extern void parse_module_check_name(char const *src, char const *name) ;
extern void regex_configure(resolve_service_t *res, ssexec_t *info, char const *path, char const *name) ;
extern int regex_get_file_name(char *filename, char const *str) ;
extern void regex_get_regex(char *regex, char const *str) ;
extern void regex_get_replace(char *replace, char const *str) ;
extern void regex_rename(stralloc *list, resolve_service_t *res, uint32_t element) ;
extern void regex_replace(stralloc *list, resolve_service_t *res) ;
extern int module_in_cmdline(genalloc *gares, resolve_service_t *res, char const *dir) ;
extern int module_search_service(char const *src, genalloc *gares, char const *name,uint8_t *found, char module_name[256]) ;

extern int module_path(stralloc *sdir, stralloc *mdir, char const *sv,char const *frontend_src, uid_t owner) ;

#endif
