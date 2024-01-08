/*
 * parse.h
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

#ifndef SS_PARSE_H
#define SS_PARSE_H

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/mill.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>
#include <66/service.h>

#define parse_error_return(ret, nerr, idsec, idkey) do { \
    parse_error(nerr, idsec, idkey) ; \
    return ret ; \
} while (0)

#define parse_error_type(type, idsec, idkey ) do { \
    if (type == TYPE_MODULE) \
        log_warn_return(LOG_EXIT_ONE, "key: ", get_key_by_enum(idsec, idkey), " is not valid for type ", get_key_by_enum(ENUM_TYPE, type), " -- ignoring it") ; \
} while (0)

/** mill configuration */
extern parse_mill_t MILL_GET_SECTION_NAME ;
extern parse_mill_t MILL_GET_KEY ;
extern parse_mill_t MILL_GET_VALUE ;

/** freed and cleanup*/
extern void ssexec_enable_cleanup(void) ;
extern void parse_cleanup(resolve_service_t *res, char const *tmpdir, uint8_t force) ;

/** main */
extern void start_parser(char const *sv, ssexec_t *info, uint8_t disable_module, char const *directory_forced) ;

extern void parse_service(struct resolve_hash_s **href, char const *sv, ssexec_t *info, uint8_t force, uint8_t conf) ;
extern int parse_frontend(char const *sv, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns, char const *intree) ;
extern int parse_interdependences(char const *service, char const *list, unsigned int listlen, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns, char const *intree) ;
extern void parse_append_logger(struct resolve_hash_s **hres, resolve_service_t *res, ssexec_t *info) ;

/** split */
extern int parse_section(stralloc *secname, char const *str, size_t *pos) ;
extern int parse_split_from_section(resolve_service_t *res, stralloc *secname, char *str, char const *svname) ;
extern int parse_contents(resolve_wrapper_t_ref wres, char const *contents, char const *svname) ;
extern int parse_compute_list(resolve_wrapper_t_ref wres, stralloc *sa, uint32_t *res, uint8_t opts) ;

/** store */
extern int parse_store_g(resolve_service_t *res, char *store, int idsec, int idkey) ;
extern int parse_store_main(resolve_service_t *res, char *store, int idsec, int idkey) ;
extern int parse_store_start_stop(resolve_service_t *res, char *store, int idsec, int idkey) ;
extern int parse_store_logger(resolve_service_t *res, char *store, int idsec, int idkey) ;
extern int parse_store_environ(resolve_service_t *res, char *store, int idsec, int idkey) ;
extern int parse_store_regex(resolve_service_t *res, char *store, int idsec, int idkey) ;

/** helper */
extern int parse_line_g(char *store, parse_mill_t *config, char const *str, size_t *pos) ;
extern int parse_parentheses(char *store, char const *str, size_t *pos) ;
extern int parse_clean_runas(char const *str, int idsec, int idkey) ;
extern int parse_clean_quotes(char *str) ;
extern int parse_clean_list(stralloc *sa, char const *str) ;
extern int parse_clean_line(char *str)  ;
extern int parse_mandatory(resolve_service_t *res) ;
extern void parse_error(int ierr, int idsec, int idkey) ;
extern void parse_rename_interdependences(resolve_service_t *res, char const *prefix, struct resolve_hash_s **hres, ssexec_t *info) ;
extern void parse_db_migrate(resolve_service_t *res, ssexec_t *info) ;

/** module */
extern void parse_module(resolve_service_t *res, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force) ;

/** resolve */
extern void parse_compute_resolve(resolve_service_t *res, ssexec_t *info) ;
extern uint32_t compute_src_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_live_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_status(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_scan_dir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_state_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *folder) ;
extern uint32_t compute_pipe_service(resolve_wrapper_t_ref wres, ssexec_t *info, char const *name) ;
extern uint32_t compute_log_dir(resolve_wrapper_t_ref wres, resolve_service_t *res) ;

#endif
