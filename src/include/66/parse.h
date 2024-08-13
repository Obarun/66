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

#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/enum.h>

#define parse_error_return(ret, nerr, sid, list, idkey) do { \
    parse_error(nerr, sid, list, idkey) ; \
    return ret ; \
} while (0)

#define parse_error_type(type, list, idkey) do { \
    if (type == TYPE_MODULE) \
        log_warn_return(LOG_EXIT_ONE, "key: ", get_key_by_enum(list, idkey), " is not valid for type ", get_key_by_enum(list_type, type), " -- ignoring it") ; \
} while (0)

/** lexer configuration */
extern lexer_config LEXER_CONFIG_SECTION ;

extern lexer_config LEXER_CONFIG_QUOTE ;

extern lexer_config LEXER_CONFIG_INLINE ;

extern lexer_config LEXER_CONFIG_LIST ;

extern lexer_config LEXER_CONFIG_KEY ;

/** freed and cleanup*/
extern void ssexec_enable_cleanup(void) ;
extern void parse_cleanup(resolve_service_t *res, char const *tmpdir, uint8_t force) ;

/** main */
extern void start_parser(char const *sv, ssexec_t *info, uint8_t disable_module, char const *directory_forced) ;
extern void parse_service(struct resolve_hash_s **href, char const *sv, ssexec_t *info, uint8_t force, uint8_t conf) ;
extern int parse_frontend(char const *sv, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns, char const *intree, resolve_service_t *moduleres) ;
extern int parse_interdependences(char const *service, char const *list, unsigned int listlen, struct resolve_hash_s **hres, ssexec_t *info, uint8_t force, uint8_t conf, char const *forced_directory, char const *main, char const *inns, char const *intree, resolve_service_t *moduleres) ;
extern void parse_create_logger(struct resolve_hash_s **hres, resolve_service_t *res, ssexec_t *info) ;

/** split */
extern int parse_section_main(resolve_service_t *res, const char *str) ;
extern int parse_section_start(resolve_service_t *res, const char *str) ;
extern int parse_section_stop(resolve_service_t *res, const char *str) ;
extern int parse_section_logger(resolve_service_t *res, const char *str) ;
extern int parse_section_environment(resolve_service_t *res, const char *str) ;
extern int parse_section_regex(resolve_service_t *res, const char *str) ;
extern int parse_section(resolve_service_t *res, char const *str, key_description_t const *list, const int sid) ;
extern int parse_contents(resolve_service_t *res, char const *str) ;
extern int parse_compute_list(resolve_wrapper_t_ref wres, stack *store, uint32_t *res, uint8_t opts) ;

/** store */
extern int parse_store_g(resolve_service_t *res, stack *store, const int sid, key_description_t const *list, const int idkey) ;
extern int parse_store_main(resolve_service_t *res, stack *store, int idsec, int idkey) ;
extern int parse_store_start_stop(resolve_service_t *res, stack *store, int idsec, int idkey) ;
extern int parse_store_logger(resolve_service_t *res, stack *store, int idsec, int idkey) ;
extern int parse_store_environ(resolve_service_t *res, stack *store, int idsec, int idkey) ;
extern int parse_store_regex(resolve_service_t *res, stack *store, int idsec, int idkey) ;

/** helper */
extern int parse_get_section(lexer_config *acfg, unsigned int *ncfg, char const *str, size_t len) ;
extern int parse_key(stack *key, lexer_config *cfg, key_description_t const *list) ;
extern int parse_value(stack *store, lexer_config *kcfg, const int sid, key_description_t const *list, int const kid) ;
extern int parse_list(stack *stk) ;
extern int parse_bracket(stack *store, const char *line, const int sid) ;
extern int parse_clean_runas(char const *str, int idsec, int idkey) ;
extern int parse_get_value_of_key(stack *store, char const *str, const int sid, key_description_t const *list, const int kid) ;
extern int parse_mandatory(resolve_service_t *res) ;
extern void parse_error(int ierr, int idsec, key_description_t const *list, int idkey) ;
extern void parse_rename_interdependences(resolve_service_t *res, char const *prefix, struct resolve_hash_s **hres, ssexec_t *info) ;
extern void parse_db_migrate(resolve_service_t *res, ssexec_t *info) ;
extern void parse_copy_to_source(char const *dst, char const *src, resolve_service_t *res, uint8_t force) ;

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
extern void parse_compute_scripts(resolve_service_t *res) ;

#endif
