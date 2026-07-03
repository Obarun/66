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
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/ssexec.h>

/** Resolve the StdIn/StdOut/StdErr triplet: each fd's final type and
 * destination is derived from the others and from whether the service keeps a
 * logger. Extracted verbatim from parse_mandatory(); behaviour unchanged. */
void parse_io_resolve(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    resolve_service_addon_io_type_t_ref in = &res->io.fdin ;
    resolve_service_addon_io_type_t_ref out = &res->io.fdout ;
    resolve_service_addon_io_type_t_ref err = &res->io.fderr ;
    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!res->logger.want) {
        /**
         * res->logger.want may significate two things:
         *  - !log was set at Options key.
         *  - this the resolve file of the logger itself.
         *
         * User may have define the Stdxxx keys or the keys
         * is not define at all.
         * We keep that except for the 66log type. */
        if (!res->islog) {

            if (in->type == E_PARSER_IO_TYPE_66LOG || in->type == E_PARSER_IO_TYPE_NOTSET)
                in->type = E_PARSER_IO_TYPE_PARENT ;
            if (out->type == E_PARSER_IO_TYPE_66LOG || out->type == E_PARSER_IO_TYPE_NOTSET)
                out->type = E_PARSER_IO_TYPE_PARENT ;
            if (err->type == E_PARSER_IO_TYPE_66LOG || err->type == E_PARSER_IO_TYPE_NOTSET)
                err->type = E_PARSER_IO_TYPE_PARENT ;

        } else {

            /** This is the resolve file of the logger itself.
             * This definition is only made here to provide convenient API.
             * We are in parse process and the next call of the parse_create_logger
             * will also set the Stdxxx key with the same as follow. */
            in->type = out->type = E_PARSER_IO_TYPE_66LOG ;
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
                case E_PARSER_IO_TYPE_66LOG:
                case E_PARSER_IO_TYPE_INHERIT:
                case E_PARSER_IO_TYPE_NULL:
                case E_PARSER_IO_TYPE_PARENT:
                case E_PARSER_IO_TYPE_CLOSE:
                        break ;

                case E_PARSER_IO_TYPE_NOTSET:
                    if (out->type == E_PARSER_IO_TYPE_NOTSET || out->type == E_PARSER_IO_TYPE_66LOG) {
                        in->type = E_PARSER_IO_TYPE_66LOG ;
                        in->destination = compute_pipe_service(wres, info, SS_FDHOLDER) ;
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
                case E_PARSER_IO_TYPE_66LOG:
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
                    if (in->type == E_PARSER_IO_TYPE_TTY || in->type == E_PARSER_IO_TYPE_66LOG) {
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

                    out->type = in->type = E_PARSER_IO_TYPE_66LOG ;
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

    if (res->logger.want) {
        // avoid to call parse_create_logger
        if (in->type != E_PARSER_IO_TYPE_66LOG && out->type != E_PARSER_IO_TYPE_66LOG)
            res->logger.want = 0 ;
    }
}
