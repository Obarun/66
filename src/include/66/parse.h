/*
 * parse.h
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_PARSE_H
#define SS_PARSE_H

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/enum.h>
#include <66/enum_parser.h>

#define parse_error_return(ret, nerr, table) do { \
    parse_error(nerr, table) ; \
    return ret ; \
} while (0)

#define parse_error_type(type, list, idkey) do { \
    if (type == E_PARSER_TYPE_MODULE) \
        log_warn_return(LOG_EXIT_ONE, "key: ", enum_to_key(list, idkey), " is not valid for type ", enum_to_key(enum_list_parser_type, type), " -- ignoring it") ; \
} while (0)

typedef struct parse_validator_s parse_validator_t, *parse_validator_t_ref ;
struct parse_validator_s
{
    char const *frontend ;                       // the static frontend text
    size_t off[E_PARSER_SECTION_ENDOFKEY] ;      // per-section content offset
    size_t len[E_PARSER_SECTION_ENDOFKEY] ;      // per-section content length
    uint8_t present[E_PARSER_SECTION_ENDOFKEY] ; // 1 if the section exists
} ;

#define SS_PARSE_STORE_NKEY 32 // must be >= every SECTION_<X>_ENDOFKEY (largest is [Main])

typedef struct parse_store_s parse_store_t, *parse_store_t_ref ;
struct parse_store_s
{
    char const *frontend ;                                            // static source text
    strbuf arena ;                                                    // raw values, NUL-separated
    uint32_t off[E_PARSER_SECTION_ENDOFKEY][SS_PARSE_STORE_NKEY] ;    // (sid,kid) -> value offset in arena
    uint32_t len[E_PARSER_SECTION_ENDOFKEY][SS_PARSE_STORE_NKEY] ;    // (sid,kid) -> value length (bytes)
    uint8_t present[E_PARSER_SECTION_ENDOFKEY][SS_PARSE_STORE_NKEY] ; // (sid,kid) -> written by the user ?
} ;

extern lexer_config LEXER_CONFIG_SECTION ;
extern lexer_config LEXER_CONFIG_QUOTE ;
extern lexer_config LEXER_CONFIG_INLINE ;
extern lexer_config LEXER_CONFIG_LIST ;
extern lexer_config LEXER_CONFIG_KEY ;

/** the parse context. The caller of parse_frontend fills the environment fields
 * (where and how to parse); parse_names() fills it for the operands */
typedef struct parse_build_ctx_s parse_build_ctx_t, *parse_build_ctx_t_ref ;
struct parse_build_ctx_s
{
    hash_t *hres ;
    ssexec_t *info ;
    char const *forced_directory ;
    char const *inns ;
    char const *intree ;
    resolve_service_t *moduleres ;
    uint8_t conf ;
    uint32_t flags ; // PARSE_*
    service_graph_t const *g ; // the graph of the system: what is parsed already
    strbuf *msg ;
    parse_store_t *st ;
    strbuf *copyfrom ; // where parse_core() keeps the CopyFrom it resolves
} ;

#define PARSE_REFUSE(ctx, ...) ((ctx)->msg->len ? 0 : (auto_strbuf((ctx)->msg, __VA_ARGS__) ? 0 : -1))

#define PARSE_FORCE (1 << 0)
#define PARSE_CONF (1 << 1)
#define PARSE_NODEPS (1 << 2)
#define PARSE_MISSING (1 << 3)
#define PARSE_NOFROM (1 << 4)
#define PARSE_FRONTEND (1 << 5)
#define PARSE_LEAVEGONE (1 << 6)

extern int parse_names(hash_t *hres, service_graph_t const *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flags, strbuf *msg) ;
extern int parse_check(hash_t *hres, ssexec_t *info, strbuf *msg) ;
extern int parse_dropped(bitset32_t *dropped, hash_t *hres, service_graph_t const *g) ;
extern int parse_write(hash_t *hres, ssexec_t *info) ;
extern int parse_frontend(char const *sv, parse_build_ctx_t ctx) ;
extern int parse_interdependences(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_dependency_name(char const *name, char const *service, parse_build_ctx_t *ctx) ;
extern int parse_create_logger(hash_t *hres, struct resolve_hash_s *c, ssexec_t *info) ;

/** validator */
extern int parse_validator_init(parse_validator_t *v, char const *frontend) ;

/** two-pass store */
extern int parse_store_build(parse_store_t *st, char const *frontend) ;
extern char const *parse_store_get(parse_store_t *st, uint32_t sid, uint32_t kid, size_t *len) ;
extern uint8_t parse_store_present(parse_store_t *st, uint32_t sid, uint32_t kid) ;
extern void parse_store_free(parse_store_t *st) ;

/** list */
extern int parse_compute_list(resolve_wrapper_t_ref wres, strbuf *store, uint32_t *res, uint8_t opts) ;

/** helper */
extern int parse_get_section(lexer_config *acfg, unsigned int *ncfg, char const *str, size_t len) ;
extern int parse_key(strbuf *key, lexer_config *cfg, resolve_enum_table_t table) ;
extern int parse_value(strbuf *store, lexer_config *kcfg, resolve_enum_table_t table) ;
extern int parse_list(strbuf *stk) ;
extern int parse_bracket(strbuf *store, const char *line, resolve_enum_table_t table) ;
extern int parse_clean_runas(char const *str, resolve_enum_table_t table) ;
extern int parse_io(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_io_resolve(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info) ;
extern int parse_limit(parse_store_t *st, resolve_service_addon_limit_t *l, uint8_t *has_limit) ;
extern int parse_environ(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_logger(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_regex(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_event(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_event_source(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_core(resolve_service_t *res, char const *sv, char const *svname, char const *svsrc, size_t svlen, int insta, char const *instaname, parse_build_ctx_t *ctx) ;

extern int parse_build(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_execute(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;
extern int parse_dependencies(parse_store_t *st, resolve_service_addon_dependencies_t *dep, char const *service) ;
extern void parse_error(int ierr, resolve_enum_table_t table) ;
extern int parse_rename_interdependences(resolve_service_addon_dependencies_t *dep, char const *prefix, hash_t *hres, ssexec_t *info) ;
extern struct resolve_hash_s *parse_get_hashname(char *store, hash_t *hres, char const *name, char const *ns) ;

/** module */
extern int parse_module(struct resolve_hash_s *c, parse_build_ctx_t *ctx) ;

/** resolve */
extern uint32_t compute_src_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_live_servicedir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_status(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_scan_dir(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern uint32_t compute_state_dir(resolve_wrapper_t_ref wres, ssexec_t *info, char const *folder) ;
extern uint32_t compute_pipe_service(resolve_wrapper_t_ref wres, ssexec_t *info, char const *name) ;
extern int parse_compute_live(resolve_wrapper_t_ref wres, ssexec_t *info) ;
extern int parse_copyfrom_path(char *src, char *dname, char const *frontend, char const *what) ;
extern ssize_t compute_log_dir(resolve_wrapper_t_ref wres, resolve_service_t *res, const char *destination) ;
extern void parse_compute_script(resolve_service_t *res, resolve_service_addon_execute_t *ex, uint8_t runorfinish) ;
extern void compute_wrapper_scripts(resolve_service_t *res, resolve_service_addon_execute_t *ex, uint8_t runorfinish) ;

#endif
