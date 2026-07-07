/*
 * parse_io.c
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/ssexec.h>

static void io_compute_stdin(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info, char const *line, uint32_t type)
{
    log_flow() ;

    io->fdin.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
            if (line[0] != '/')
               log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;
            io->fdin.destination = resolve_add_string(w, line) ;
            break ;

        case E_PARSER_IO_TYPE_66LOG:
            io->fdin.destination = compute_pipe_service(w, info, SS_FDHOLDER) ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io->fdin.destination = resolve_add_string(w, "/dev/null") ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
        case E_PARSER_IO_TYPE_CLOSE:
            break ;

        case E_PARSER_IO_TYPE_CONSOLE:
        case E_PARSER_IO_TYPE_FILE:
        case E_PARSER_IO_TYPE_SYSLOG:
        case E_PARSER_IO_TYPE_INHERIT:
        default:
            io->fdin.type = E_PARSER_IO_TYPE_NOTSET ;
            break ;
    }
}

static void io_compute_stdout(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, char const *line, uint32_t type)
{
    log_flow() ;

    io->fdout.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
        case E_PARSER_IO_TYPE_FILE:
            if (line[0] != '/')
                log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;
            io->fdout.destination = resolve_add_string(w, line) ;
            break ;

        case E_PARSER_IO_TYPE_CONSOLE:
            io->fdout.destination = resolve_add_string(w, "/sys/class/tty/tty0/active") ;
            break ;

        case E_PARSER_IO_TYPE_66LOG:
            // "s6log" is the deprecated spelling of "66log" -- both mean the bare keyword (default dir)
            if (!strcmp(line, enum_str_parser_io_type[E_PARSER_IO_TYPE_66LOG]) || !strcmp(line, "s6log"))
                io->fdout.destination = compute_log_dir(w, res, 0) ;
            else
                io->fdout.destination = compute_log_dir(w, res, line) ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io->fdout.destination = resolve_add_string(w, "/dev/null") ;
            break ;

        case E_PARSER_IO_TYPE_SYSLOG:
            io->fdout.destination = resolve_add_string(w, "/dev/log") ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
        case E_PARSER_IO_TYPE_CLOSE:
            break ;

        case E_PARSER_IO_TYPE_INHERIT:
        default:
            io->fdout.type = E_PARSER_IO_TYPE_NOTSET ;
            break ;
    }
}

static void io_compute_stderr(resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, char const *line, uint32_t type)
{
    log_flow() ;

    io->fderr.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
        case E_PARSER_IO_TYPE_FILE:
            if (line[0] != '/')
                log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;
            io->fderr.destination = resolve_add_string(w, line) ;
            break ;

        case E_PARSER_IO_TYPE_CONSOLE:
            io->fderr.destination = resolve_add_string(w, "/sys/class/tty/tty0/active") ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io->fderr.destination = resolve_add_string(w, "/dev/null") ;
            break ;

        case E_PARSER_IO_TYPE_SYSLOG:
            io->fderr.destination = resolve_add_string(w, "/dev/log") ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
        case E_PARSER_IO_TYPE_CLOSE:
        case E_PARSER_IO_TYPE_INHERIT:
            break ;

        case E_PARSER_IO_TYPE_66LOG:
        default:
            io->fderr.type = E_PARSER_IO_TYPE_NOTSET ;
            break ;
    }
}

static int io_parse_one(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info, resolve_enum_table_t table, char const *line)
{
    log_flow() ;

    size_t len = strlen(line) ;
    _alloc_sbl_(stk, len) ;
    ssize_t delim = get_len_until(line, ':'), type = -1 ;

    if (delim + 2 >= (ssize_t)len)
        parse_error_return(0, 10, table) ;

    char *stype = (char *)line ;
    char stype_buf[delim > 0 ? (size_t)delim + 1 : 1] ;

    if (delim > 0) {
        memcpy(stype_buf, line, delim) ;
        stype_buf[delim] = 0 ;
        stype = stype_buf ;
    }

    type = key_to_enum(enum_list_parser_io_type, stype) ;

    if (type == -1) {
        // "s6log" is deprecated: accept it as an alias of "66log"
        if (!strcmp(stype, "s6log")) {
            log_warn("the 's6log' io type is deprecated -- use '66log' instead; converting it automatically") ;
            type = E_PARSER_IO_TYPE_66LOG ;
        } else {
            log_warn("invalid type for ", *table.u.parser.list[table.u.parser.id].name, " key in section main -- applying default") ;
            return 1 ;
        }
    }

    stk.len = 0 ;

    if (delim > 0) {
        if (!sbl_addb(&stk, line + delim + 1, len - delim + 1))
            log_die_nomem("stack") ;
    } else {
        if (!sbl_addb(&stk, line, len))
            log_die_nomem("stack") ;
    }

    switch(table.u.parser.id) {

        case E_PARSER_SECTION_MAIN_STDIN:
            io_compute_stdin(res, io, w, info, stk.s, (uint32_t)type) ;
            break ;

        case E_PARSER_SECTION_MAIN_STDOUT:
            io_compute_stdout(res, io, w, stk.s, (uint32_t)type) ;
            break ;

        case E_PARSER_SECTION_MAIN_STDERR:
            io_compute_stderr(io, w, stk.s, (uint32_t)type) ;
            break ;

        default:
            break ;
    }
    return 1 ;
}

void parse_io_resolve(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info)
{
    log_flow() ;

    resolve_service_addon_io_type_t_ref in = &io->fdin ;
    resolve_service_addon_io_type_t_ref out = &io->fdout ;
    resolve_service_addon_io_type_t_ref err = &io->fderr ;

    if (!res->logger) {

        if (!res->islog) {

            if (in->type == E_PARSER_IO_TYPE_66LOG || in->type == E_PARSER_IO_TYPE_NOTSET)
                in->type = E_PARSER_IO_TYPE_PARENT ;
            if (out->type == E_PARSER_IO_TYPE_66LOG || out->type == E_PARSER_IO_TYPE_NOTSET)
                out->type = E_PARSER_IO_TYPE_PARENT ;
            if (err->type == E_PARSER_IO_TYPE_66LOG || err->type == E_PARSER_IO_TYPE_NOTSET)
                err->type = E_PARSER_IO_TYPE_PARENT ;

        } else {

            in->type = out->type = E_PARSER_IO_TYPE_66LOG ;
            in->destination = compute_pipe_service(w, info, SS_FDHOLDER) ;
            if (!out->destination)
                out->destination = compute_log_dir(w, res, 0) ;

            err->type = E_PARSER_IO_TYPE_INHERIT ;
            err->destination = out->destination ;
        }

    } else {

        {
            switch(in->type) {

                case E_PARSER_IO_TYPE_TTY:
                case E_PARSER_IO_TYPE_CONSOLE:
                case E_PARSER_IO_TYPE_SYSLOG:
                case E_PARSER_IO_TYPE_FILE:
                case E_PARSER_IO_TYPE_66LOG:
                case E_PARSER_IO_TYPE_INHERIT:
                case E_PARSER_IO_TYPE_NULL:
                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                        break ;

                case E_PARSER_IO_TYPE_NOTSET:
                    if (out->type == E_PARSER_IO_TYPE_NOTSET || out->type == E_PARSER_IO_TYPE_66LOG) {
                        in->type = E_PARSER_IO_TYPE_66LOG ;
                        in->destination = compute_pipe_service(w, info, SS_FDHOLDER) ;
                        break ;
                    }

                    in->type = E_PARSER_IO_TYPE_PARENT ;
                    break ;

                default:
                    break ;
            }
        }

        if (in->type == E_PARSER_IO_TYPE_66LOG) {
            out->type = in->type ;
            if (!out->destination)
                out->destination = compute_log_dir(w, res, 0) ;
        }

        {
            switch(out->type) {
                case E_PARSER_IO_TYPE_TTY:
                    if (in->type == E_PARSER_IO_TYPE_TTY)
                        out->destination = in->destination ;
                    break ;
                case E_PARSER_IO_TYPE_FILE:
                case E_PARSER_IO_TYPE_CONSOLE:
                case E_PARSER_IO_TYPE_66LOG:
                case E_PARSER_IO_TYPE_SYSLOG:
                case E_PARSER_IO_TYPE_INHERIT:
                    break ;

                case E_PARSER_IO_TYPE_NULL:
                    if (in->type == E_PARSER_IO_TYPE_NULL) {
                        out->type = E_PARSER_IO_TYPE_INHERIT ;
                        break ;
                    }
                    break ;

                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                    break ;

                case E_PARSER_IO_TYPE_NOTSET:
                    if (in->type == E_PARSER_IO_TYPE_TTY || in->type == E_PARSER_IO_TYPE_66LOG) {
                        out->type = in->type ;
                        out->destination = (in->type == E_PARSER_IO_TYPE_TTY) ? in->destination : compute_log_dir(w, res, 0) ;
                        break ;
                    }

                    if (in->type == E_PARSER_IO_TYPE_NULL) {
                        out->type = E_PARSER_IO_TYPE_INHERIT ;
                        break ;
                    }

                    if (in->type == E_PARSER_IO_TYPE_PARENT) {
                        out->type = in->type ;
                        break ;
                    }

                    if (in->type == E_PARSER_IO_TYPE_CLOSE) {
                        out->type = E_PARSER_IO_TYPE_PARENT ;
                        break ;
                    }

                    out->type = in->type = E_PARSER_IO_TYPE_66LOG ;
                    out->destination = compute_log_dir(w, res, 0) ;
                    in->destination = compute_pipe_service(w, info, SS_FDHOLDER) ;

                default:
                    break ;
            }
        }

        if ((err->type == out->type) && (in->type == out->type)) {
            err->type = E_PARSER_IO_TYPE_INHERIT ;
            err->destination = out->destination ;
        }

        if (out->type == E_PARSER_IO_TYPE_SYSLOG) {
            err->type = out->type ;
            err->destination = out->destination ;
        }

        {
            switch(err->type) {

                case E_PARSER_IO_TYPE_TTY:
                case E_PARSER_IO_TYPE_FILE:
                case E_PARSER_IO_TYPE_CONSOLE:
                    break ;
                case E_PARSER_IO_TYPE_66LOG:
                    err->destination = out->destination ;
                    break ;
                case E_PARSER_IO_TYPE_SYSLOG:
                case E_PARSER_IO_TYPE_NULL:
                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                    break ;
                case E_PARSER_IO_TYPE_INHERIT:
                case E_PARSER_IO_TYPE_NOTSET:
                    err->type = E_PARSER_IO_TYPE_INHERIT ;
                    err->destination = out->destination ;
                    break ;
                default:
                    break ;
            }
        }
    }
}

int parse_io(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_io_t *io = &c->io ;
    ssexec_t *info = ctx->info ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_IO, io) ;
    resolve_init(w) ; // offset 0 = "" convention

    uint32_t const keys[3] = {
        E_PARSER_SECTION_MAIN_STDIN,
        E_PARSER_SECTION_MAIN_STDOUT,
        E_PARSER_SECTION_MAIN_STDERR,
    } ;

    for (unsigned int i = 0 ; i < 3 ; i++) {

        if (!parse_store_present(st, E_PARSER_SECTION_MAIN, keys[i]))
            continue ;

        char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, keys[i], 0) ;

        resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
        table.u.parser.id = keys[i] ;

        if (!io_parse_one(res, io, w, info, table, v)) {
            free(w) ;
            return 0 ;
        }
    }

    parse_io_resolve(res, io, w, info) ;

    /* a 66log destination directory must be owned by the logger runas: keep it on
     * the io addon so 66-execute can chown the directory (classic and oneshot),
     * without reading a separate logger addon. */
    if (io->fdout.type == E_PARSER_IO_TYPE_66LOG && c->logger.execute.run.runas)
        io->runas = resolve_add_string(w, c->logger.sa.s + c->logger.execute.run.runas) ;

    free(w) ;

    res->has_io = 1 ;

    return 1 ;
}
