/*
 * parse_execute.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h> // free
#include <string.h>
#include <sys/resource.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/caps.h>

static int store_list_sid(strbuf *stk, parse_store_t *st, uint32_t sid, uint32_t kid)
{
    size_t len = 0 ;
    char const *v = parse_store_get(st, sid, kid, &len) ;

    if (!strbuf_copyb(stk, v, len))
        log_die_nomem("stack") ;

    return parse_list(stk) ;
}

static int get_shebang(strbuf *stk, char const *line)
{
    size_t len = strlen(line) ;
    uint32_t i = 0 ;

    while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')
        i++ ;

    if (i >= len || line[i] != '#' || line[i + 1] != '!')
        return 0 ;

    if (!sbl_addb(stk, line + i, len - i))
        log_warnsys_return(LOG_EXIT_LESSONE, "stack add") ;

    return 1 ;
}

static int execute_shebang(resolve_service_addon_execute_t *ex, resolve_wrapper_t_ref wres, resolve_service_addon_scripts_t *script)
{
    if (!script->run_user)
        return 1 ;

    size_t len = strlen(ex->sa.s + script->run_user) ;
    _alloc_sbl_(stk, len) ;

    int r = get_shebang(&stk, ex->sa.s + script->run_user) ;
    if (r < 0)
        return 0 ;
    if (r) {
        script->run_user = resolve_add_string(wres, stk.s) ;
        script->build = resolve_add_string(wres, "custom") ;
    }

    return 1 ;
}

static int parse_execute_main(parse_store_t *st, resolve_service_t *res, resolve_service_addon_execute_t *ex)
{
    log_flow() ;

    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_MAIN_ENDOFKEY ; kid++) {

        if (!st->present[E_PARSER_SECTION_MAIN][kid])
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, kid, &len) ;

        switch (kid) {

            case E_PARSER_SECTION_MAIN_NOTIFY:

                parse_error_type(res->type, enum_list_parser_section_main, kid) ;

                if (!u32_scan_strict(v, &ex->notify))
                    parse_error_return(0, 3, table) ;

                if (ex->notify < 3)
                    parse_error_return(0, 0, table) ;

                break ;

            case E_PARSER_SECTION_MAIN_DEATH:

                if (!u32_scan_strict(v, &ex->maxdeath))
                    parse_error_return(0, 3, table) ;

                if (ex->maxdeath > 16)
                    parse_error_return(0, 0, table) ;

                break ;

            case E_PARSER_SECTION_MAIN_DEATHTIME:

                if (!u32_scan_strict(v, &ex->maxdeathtime))
                    parse_error_return(0, 3, table) ;

                break ;

            case E_PARSER_SECTION_MAIN_SIGNAL:
                {
                    parse_error_type(res->type, enum_list_parser_section_main, kid) ;
                    int t = 0 ;

                    if (!sig_parse(v, &t))
                        parse_error_return(0, 3, table) ;

                    ex->downsignal = (uint32_t)t ;
                }
                break ;

            case E_PARSER_SECTION_MAIN_FLAGS:
                {
                    parse_error_type(res->type, enum_list_parser_section_main, kid) ;

                    _alloc_sbl_(stk, len + 1) ;

                    if (!store_list_sid(&stk, st, E_PARSER_SECTION_MAIN, kid))
                        parse_error_return(0, 8, table) ;

                    size_t pos = 0 ;
                    FOREACH_SBL(&stk, pos) {

                        int r = key_to_enum(enum_list_parser_flags, stk.s + pos) ;

                        if (r == -1)
                            parse_error_return(0, 0, table) ;

                        if (r == E_PARSER_FLAGS_DOWN)
                            ex->down = 1 ; // 0 means not enabled ; EARLIER is parse_core's
                    }
                }
                break ;

            /* the [Main] timeouts are deprecated ; [Start]/[Stop] win over them */
            case E_PARSER_SECTION_MAIN_TIMESTART:

                parse_error_type(res->type, enum_list_parser_section_main, kid) ;
                log_1_warn("key TimeoutStart at section [Main] is deprecated -- declare it at section [Start] instead") ;

                if (!u32_scan_strict(v, &ex->timeout.start))
                    parse_error_return(0, 3, table) ;
                break ;

            case E_PARSER_SECTION_MAIN_TIMESTOP:

                parse_error_type(res->type, enum_list_parser_section_main, kid) ;

                log_1_warn("key TimeoutStop at section [Main] is deprecated -- declare it at section [Stop] instead") ;
                if (!u32_scan_strict(v, &ex->timeout.stop))
                    parse_error_return(0, 3, table) ;
                break ;

            default: break ;
        }
    }

    return 1 ;
}

static int parse_execute_section(parse_store_t *st, resolve_service_t *res, resolve_service_addon_execute_t *ex, resolve_wrapper_t_ref wres)
{
    log_flow() ;

    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_EXECUTE_ZERO ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_EXECUTE_ENDOFKEY ; kid++) {

        if (!st->present[E_PARSER_SECTION_EXECUTE][kid])
            continue ;

        table.u.parser.id = kid ;
        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_EXECUTE, kid, &len) ;

        switch (kid) {

            case E_PARSER_SECTION_EXECUTE_BLOCK_PRIVILEGES:

                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
                if (v[0] == 'T' || v[0] == 't' || v[0] == '1')
                    ex->blockprivileges = 1 ;

                break ;

            case E_PARSER_SECTION_EXECUTE_UMASK:
                {
                    parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
                    uint32_t mode ;

                    /** UMask is octal notation (e.g. 022): scan base 8, not base 10. */
                    if (!u32_scan_strict_base(v, &mode, 8))
                        parse_error_return(0, 3, table) ;

                    if (mode > 0777)
                        parse_error_return(0, 0, table) ;

                    ex->umask = mode ;
                    ex->want_umask = 1 ;
                }
                break ;

            case E_PARSER_SECTION_EXECUTE_NICE:
                {
                    parse_error_type(res->type, enum_list_parser_section_execute, kid) ;
                    int64_t n = 0 ;

                    if (!i64_scan_base_max(v, &n, 10, INT64_MAX))
                        parse_error_return(0, 3, table) ;

                    if (n < -20 || n > 19)
                        parse_error_return(0, 0, table) ;

                    ex->nice = (uint32_t)(20 - n) ;
                    ex->want_nice = 1 ;
                }
                break ;

            case E_PARSER_SECTION_EXECUTE_CHDIR:

                parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                if (v[0] != '/')
                    parse_error_return(0, 4, table) ;

                ex->chdir = resolve_add_string(wres, v) ;
                break ;

            case E_PARSER_SECTION_EXECUTE_CAPS_BOUND:
                {
                    parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                    _alloc_sbl_(store, len + 1) ;

                    if (!store_list_sid(&store, st, E_PARSER_SECTION_EXECUTE, kid))
                        parse_error_return(0, 8, table) ;

                    if (store.len && !res->owner) {

                        _alloc_sbl_(stk, 2048) ; // ~ (70 CAPS * 30)

                        parse_store_caps(&stk, &store, &ex->ncapsbound) ;

                        if (stk.len)
                            ex->capsbound = resolve_add_string(wres, stk.s) ;
                    }
                }
                break ;

            case E_PARSER_SECTION_EXECUTE_CAPS_AMBIENT:
                {
                    parse_error_type(res->type, enum_list_parser_section_execute, kid) ;

                    _alloc_sbl_(store, len + 1) ;

                    if (!store_list_sid(&store, st, E_PARSER_SECTION_EXECUTE, kid))
                        parse_error_return(0, 8, table) ;

                    if (store.len) {

                        _alloc_sbl_(stk, 2048) ; // ~ (70 CAPS * 30)

                        parse_store_caps(&stk, &store, &ex->ncapsambient) ;

                        if (stk.len)
                            ex->capsambient = resolve_add_string(wres, stk.s) ;
                    }
                }
                break ;

            default: break ;
        }
    }

    return 1 ;
}

static int parse_execute_startstop(parse_store_t *st, resolve_service_t *res, resolve_service_addon_execute_t *ex, resolve_wrapper_t_ref wres, uint32_t sid)
{
    log_flow() ;

    resolve_enum_table_t table = sid == E_PARSER_SECTION_START ? \
        (resolve_enum_table_t)E_TABLE_PARSER_SECTION_START_ZERO : \
        (resolve_enum_table_t)E_TABLE_PARSER_SECTION_STOP_ZERO ;

    resolve_service_addon_scripts_t *script = sid == E_PARSER_SECTION_START ? &ex->run : &ex->finish ;
    uint8_t runorfinish = sid == E_PARSER_SECTION_START ? 1 : 0 ;

    for (uint32_t kid = 0 ; kid < E_PARSER_SECTION_STARTSTOP_ENDOFKEY ; kid++) {

        if (!st->present[sid][kid])
            continue ;

        table.u.parser.id = kid ;
        char const *v = parse_store_get(st, sid, kid, 0) ;

        switch (kid) {

            case E_PARSER_SECTION_STARTSTOP_BUILD:
                log_warn("key Build is deprecated and ignored -- declare a shebang (#!) at the start of the Execute field instead") ;
                break ;

            case E_PARSER_SECTION_STARTSTOP_RUNAS:
                {
                    char tmp[strlen(v) + 1] ;
                    auto_strings(tmp, v) ;

                    if (!parse_clean_runas(tmp, table))
                        return 0 ;

                    script->runas = resolve_add_string(wres, tmp) ;
                }
                break ;

            case E_PARSER_SECTION_STARTSTOP_EXEC:
                script->run_user = resolve_add_string(wres, v) ;
                break ;

            case E_PARSER_SECTION_STARTSTOP_TIMESTART:

                if (sid != E_PARSER_SECTION_START)
                    log_warn_return(LOG_EXIT_ZERO, "key TimeoutStart is only valid at section [Start]") ;

                if (!u32_scan_strict(v, &ex->timeout.start))
                    parse_error_return(0, 3, table) ;

                break ;

            case E_PARSER_SECTION_STARTSTOP_TIMESTOP:

                if (sid != E_PARSER_SECTION_STOP)
                    log_warn_return(LOG_EXIT_ZERO, "key TimeoutStop is only valid at section [Stop]") ;

                if (!u32_scan_strict(v, &ex->timeout.stop))
                    parse_error_return(0, 3, table) ;

                break ;

            default: break ;
        }
    }

    if (sid == E_PARSER_SECTION_START && !script->run_user)
        log_warn_return(LOG_EXIT_ZERO, "key Execute at section [Start] must be set") ;

    if (!execute_shebang(ex, wres, script))
        return 0 ;

    if (script->run_user)
        parse_compute_script(res, ex, runorfinish) ;

    return 1 ;
}

int parse_execute(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_execute_t *ex = &c->execute ;
    resolve_service_addon_limit_t *l = &c->limit ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;
    uint8_t has_limit = 0 ;

    if (!parse_execute_main(st, res, ex)) { free(wres) ; return 0 ; }

    if (!parse_execute_section(st, res, ex, wres)) { free(wres) ; return 0 ; }

    if (!parse_limit(st, l, &has_limit)) { free(wres) ; return 0 ; }
    res->has_limit = has_limit ;

    if (res->type != E_PARSER_TYPE_MODULE) {
        if (!parse_execute_startstop(st, res, ex, wres, E_PARSER_SECTION_START) ||
            !parse_execute_startstop(st, res, ex, wres, E_PARSER_SECTION_STOP)) {
                free(wres) ;
                return 0 ;
        }
    }

    free(wres) ;

    res->has_execute = 1 ;

    return 1 ;
}
