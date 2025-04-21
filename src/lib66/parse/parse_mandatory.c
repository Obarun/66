/*
 * parse_mandatory.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/constants.h>

static int get_shebang(stack *stk, char const *line)
{
    size_t len = strlen(line) ;
    uint32_t i = 0 ;

    while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')
        i++ ;

    if (i >= len || line[i] != '#' || line[i + 1] != '!' || line[i + 2] != '/')
        log_warn_return(LOG_EXIT_ZERO, "invalid shebang at Execute field from section [Start]") ;

    if (!stack_add(stk, line + i, len - i))
        log_warnsys_return(LOG_EXIT_ZERO, "stack add") ;

    stack_close(stk) ;

    return 1 ;
}

int parse_mandatory(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    IO_type_t_ref in = &res->io.fdin ;
    IO_type_t_ref out = &res->io.fdout ;
    IO_type_t_ref err = &res->io.fderr ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!res->description)
        log_warn_return(LOG_EXIT_ZERO,"key Description at section [Main] must be set") ;

    if (!res->user)
        log_warn_return(LOG_EXIT_ZERO,"key User at section [Main] must be set") ;

    if (!res->version)
        log_warn_return(LOG_EXIT_ZERO,"key Version at section [Main] must be set") ;

    if (!res->logger.want) {
        /**
         * res->logger.want may significate two things:
         *  - !log was set at Options key.
         *  - this the resolve file of the logger itself.
         *
         * User may have define the Stdxxx keys or the keys
         * is not define at all.
         * We keep that except for the s6log type. */
        if (!res->islog) {

            if (in->type == E_PARSER_IO_TYPE_S6LOG || in->type == E_PARSER_IO_TYPE_NOTSET)
                in->type = E_PARSER_IO_TYPE_PARENT ;
            if (out->type == E_PARSER_IO_TYPE_S6LOG || out->type == E_PARSER_IO_TYPE_NOTSET)
                out->type = E_PARSER_IO_TYPE_PARENT ;
            if (err->type == E_PARSER_IO_TYPE_S6LOG || err->type == E_PARSER_IO_TYPE_NOTSET)
                err->type = E_PARSER_IO_TYPE_PARENT ;

        } else {

            /** This is the resolve file of the logger itself.
             * This definition is only made here to provide convenient API.
             * We are in parse process and the next call of the parse_create_logger
             * will also set the Stdxxx key with the same as follow. */
            in->type = out->type = E_PARSER_IO_TYPE_S6LOG ;
            in->destination = compute_pipe_service(wres, info, SS_FDHOLDER) ;
            if (!out->destination)
                out->destination = compute_log_dir(wres, res, 0) ;

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
                case E_PARSER_IO_TYPE_S6LOG:
                case E_PARSER_IO_TYPE_INHERIT:
                case E_PARSER_IO_TYPE_NULL:
                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                        break ;

                case E_PARSER_IO_TYPE_NOTSET:
                    if (out->type == E_PARSER_IO_TYPE_NOTSET || out->type == E_PARSER_IO_TYPE_S6LOG) {
                        in->type = E_PARSER_IO_TYPE_S6LOG ;
                        in->destination = compute_pipe_service(wres, info, SS_FDHOLDER) ;
                        break ;
                    }

                    in->type = E_PARSER_IO_TYPE_PARENT ;
                    break ;

                default:
                    break ;
            }
        }

        if (in->type == E_PARSER_IO_TYPE_S6LOG) {
            out->type = in->type ;
            if (!out->destination)
                out->destination = compute_log_dir(wres, res, 0) ;
        }

        {
            switch(out->type) {
                case E_PARSER_IO_TYPE_TTY:
                    if (in->type == E_PARSER_IO_TYPE_TTY)
                        out->destination = in->destination ;
                    break ;
                case E_PARSER_IO_TYPE_FILE:
                case E_PARSER_IO_TYPE_CONSOLE:
                case E_PARSER_IO_TYPE_S6LOG:
                case E_PARSER_IO_TYPE_SYSLOG:
                case E_PARSER_IO_TYPE_INHERIT:
                    break ;

                case E_PARSER_IO_TYPE_NULL:
                    if (in->type == E_PARSER_IO_TYPE_NULL) {
                        out->type == E_PARSER_IO_TYPE_INHERIT ;
                        break ;
                    }
                    break ;

                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                    break ;

                case E_PARSER_IO_TYPE_NOTSET:
                    if (in->type == E_PARSER_IO_TYPE_TTY || in->type == E_PARSER_IO_TYPE_S6LOG) {
                        out->type = in->type ;
                        out->destination = (in->type == E_PARSER_IO_TYPE_TTY) ? in->destination : compute_log_dir(wres, res, 0) ;
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

                    out->type = in->type = E_PARSER_IO_TYPE_S6LOG ;
                    out->destination = compute_log_dir(wres, res, 0) ;
                    in->destination = compute_pipe_service(wres, info, SS_FDHOLDER) ;

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
                case E_PARSER_IO_TYPE_S6LOG:
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

    if (res->logger.want) {
        // avoid to call parse_create_logger
        if (in->type != E_PARSER_IO_TYPE_S6LOG && out->type != E_PARSER_IO_TYPE_S6LOG)
            res->logger.want = 0 ;
    }

    switch (res->type) {

        case E_PARSER_TYPE_CLASSIC:
        case E_PARSER_TYPE_ONESHOT:

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;

            if (!strcmp(res->sa.s + res->execute.run.build, "custom")) {

                size_t len = strlen(res->sa.s + res->execute.run.run_user) ;
                _alloc_stk_(stk, len) ;

                if (!get_shebang(&stk, res->sa.s + res->execute.run.run_user))
                    return 0 ;

                res->execute.run.run_user = resolve_add_string(wres, stk.s) ;
            }

            if (res->execute.finish.run_user && !strcmp(res->sa.s + res->execute.finish.build, "custom")) {

                size_t len = strlen(res->sa.s + res->execute.finish.run_user) ;
                _alloc_stk_(stk, len) ;

                if (!get_shebang(&stk, res->sa.s + res->execute.finish.run_user))
                    return 0 ;

                res->execute.finish.run_user = resolve_add_string(wres, stk.s) ;
            }

            if (res->logger.execute.run.run_user && !strcmp(res->sa.s + res->logger.execute.run.build, "custom")) {

                size_t len = strlen(res->sa.s + res->logger.execute.run.run_user) ;
                _alloc_stk_(stk, len) ;

                if (!get_shebang(&stk, res->sa.s + res->logger.execute.run.run_user))
                    return 0 ;

                res->logger.execute.run.run_user = resolve_add_string(wres, stk.s) ;
            }

            break ;

        case E_PARSER_TYPE_MODULE:
        default:
            break ;
    }
    free(wres) ;
    return 1 ;
}
