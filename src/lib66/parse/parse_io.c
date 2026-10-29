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

// the log directory of @res in @field: 1 on success, -1 on a system error
static int set_logdir(uint32_t *field, resolve_wrapper_t_ref w, resolve_service_t *res, char const *destination)
{
    ssize_t r = compute_log_dir(w, res, destination) ;
    if (r < 0)
        return -1 ;

    *field = (uint32_t)r ;

    return 1 ;
}

static int io_compute_stdin(resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info, char const *line, uint32_t type)
{
    log_flow() ;

    io->fdin.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
            if (line[0] != '/')
               log_warn_return(LOG_EXIT_ZERO, "path must be absolute: ", line) ;
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

    return 1 ;
}

static int io_compute_stdout(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, char const *line, uint32_t type)
{
    log_flow() ;

    io->fdout.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
        case E_PARSER_IO_TYPE_FILE:
            if (line[0] != '/')
                log_warn_return(LOG_EXIT_ZERO, "path must be absolute: ", line) ;
            io->fdout.destination = resolve_add_string(w, line) ;
            break ;

        case E_PARSER_IO_TYPE_CONSOLE:
            io->fdout.destination = resolve_add_string(w, "/sys/class/tty/tty0/active") ;
            break ;

        case E_PARSER_IO_TYPE_66LOG:
            // "s6log" is the deprecated spelling of "66log" -- both mean the bare keyword (default dir)
            if (!strcmp(line, enum_str_parser_io_type[E_PARSER_IO_TYPE_66LOG]) || !strcmp(line, "s6log"))
                return set_logdir(&io->fdout.destination, w, res, 0) ;

            if (strlen(line) > SS_MAX_PATH)
                log_warn_return(LOG_EXIT_ZERO, "path is too long: ", line) ;

            return set_logdir(&io->fdout.destination, w, res, line) ;

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

    return 1 ;
}

static int io_compute_stderr(resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, char const *line, uint32_t type)
{
    log_flow() ;

    io->fderr.type = type ;

    switch(type) {

        case E_PARSER_IO_TYPE_TTY:
        case E_PARSER_IO_TYPE_FILE:
            if (line[0] != '/')
                log_warn_return(LOG_EXIT_ZERO, "path must be absolute: ", line) ;
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

    return 1 ;
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
            log_warn("invalid type for key ", *table.u.parser.list[table.u.parser.id].name, " at section ", enum_str_parser_section[table.u.parser.sid], " -- applying default") ;
            return 1 ;
        }
    }

    stk.len = 0 ;

    if (delim > 0) {
        if (!sbl_addb(&stk, line + delim + 1, len - delim + 1))
            return -1 ;
    } else {
        if (!sbl_addb(&stk, line, len))
            return -1 ;
    }

    switch(table.u.parser.sid) {

        case E_PARSER_SECTION_MAIN:
            switch(table.u.parser.id) {
                case E_PARSER_SECTION_MAIN_STDIN:
                    return io_compute_stdin(io, w, info, stk.s, (uint32_t)type) ;

                case E_PARSER_SECTION_MAIN_STDOUT:
                    return io_compute_stdout(res, io, w, stk.s, (uint32_t)type) ;

                case E_PARSER_SECTION_MAIN_STDERR:
                    return io_compute_stderr(io, w, stk.s, (uint32_t)type) ;

                default: break ;
            }
            break ;

        case E_PARSER_SECTION_EXECUTE:
            switch(table.u.parser.id) {

                case E_PARSER_SECTION_EXECUTE_STDIN:
                    return io_compute_stdin(io, w, info, stk.s, (uint32_t)type) ;

                case E_PARSER_SECTION_EXECUTE_STDOUT:
                    return io_compute_stdout(res, io, w, stk.s, (uint32_t)type) ;

                case E_PARSER_SECTION_EXECUTE_STDERR:
                    return io_compute_stderr(io, w, stk.s, (uint32_t)type) ;

                default: break ;
            }
            break ;

        default:
            break ;
    }
    return 1 ;
}

int parse_io_resolve(resolve_service_t *res, resolve_service_addon_io_t *io, resolve_wrapper_t_ref w, ssexec_t *info)
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
            if (!out->destination && set_logdir(&out->destination, w, res, 0) < 0)
                return -1 ;

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
            if (!out->destination && set_logdir(&out->destination, w, res, 0) < 0)
                return -1 ;
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
                    if (in->type == E_PARSER_IO_TYPE_TTY) {
                        out->type = in->type ;
                        out->destination = in->destination ;
                        break ;
                    }

                    if (in->type == E_PARSER_IO_TYPE_66LOG) {
                        out->type = in->type ;
                        if (set_logdir(&out->destination, w, res, 0) < 0)
                            return -1 ;
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
                    if (set_logdir(&out->destination, w, res, 0) < 0)
                        return -1 ;
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

    return 1 ;
}

int parse_io(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_io_t *io = &c->io ;
    ssexec_t *info = ctx->info ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_IO, io) ;
    resolve_init(&w) ; // offset 0 = "" convention

    static uint32_t const main_keys[3] = {
        E_PARSER_SECTION_MAIN_STDIN,
        E_PARSER_SECTION_MAIN_STDOUT,
        E_PARSER_SECTION_MAIN_STDERR,
    } ;
    static uint32_t const keys[3] = {
        E_PARSER_SECTION_EXECUTE_STDIN,
        E_PARSER_SECTION_EXECUTE_STDOUT,
        E_PARSER_SECTION_EXECUTE_STDERR,
    } ;
    static char const *const names[3] = { "StdIn", "StdOut", "StdErr" } ;

    for (unsigned int i = 0 ; i < 3 ; i++) {

        if (parse_store_present(st, E_PARSER_SECTION_MAIN, main_keys[i])) {

            log_warn("key ", names[i], " at section [Main] is deprecated -- declare it at section [Execute] instead") ;

            char const *v = parse_store_get(st, E_PARSER_SECTION_MAIN, main_keys[i], 0) ;
            resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
            table.u.parser.id = main_keys[i] ;

            int r = io_parse_one(res, io, &w, info, table, v) ;
            if (r <= 0) {
                return r ;
            }
        }

        if (parse_store_present(st, E_PARSER_SECTION_EXECUTE, keys[i])) {

            char const *v = parse_store_get(st, E_PARSER_SECTION_EXECUTE, keys[i], 0) ;
            resolve_enum_table_t table = E_TABLE_PARSER_SECTION_EXECUTE_ZERO ;
            table.u.parser.id = keys[i] ;

            int r = io_parse_one(res, io, &w, info, table, v) ;
            if (r <= 0) {
                return r ;
            }
        }
    }

    if (parse_io_resolve(res, io, &w, info) < 0) {
        return -1 ;
    }

    /* a 66log destination directory must be owned by the logger runas: keep it on
     * the io addon so 66-execute can chown the directory (classic and oneshot),
     * without reading a separate logger addon. */
    if (io->fdout.type == E_PARSER_IO_TYPE_66LOG && c->logger.execute.run.runas)
        io->runas = resolve_add_string(&w, c->logger.sa.s + c->logger.execute.run.runas) ;

    res->has_io = 1 ;

    return 1 ;
}
