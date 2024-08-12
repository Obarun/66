/*
 * parse_mandatory.c
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

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_mandatory(resolve_service_t *res)
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

            if (in->type == IO_TYPE_S6LOG || in->type == IO_TYPE_NOTSET)
                in->type = IO_TYPE_PARENT ;
            if (out->type == IO_TYPE_S6LOG || out->type == IO_TYPE_NOTSET)
                out->type = IO_TYPE_PARENT ;
            if (err->type == IO_TYPE_S6LOG || err->type == IO_TYPE_NOTSET)
                err->type = IO_TYPE_PARENT ;

        } else {

            /** This is the resolve file of the logger itself.
             * This definition is only made here to provide convenient API.
             * We are in parse process and the next call of the parse_create_logger
             * will also set the Stdxxx key with the same as follow. */
            in->type = out->type = IO_TYPE_S6LOG ;
            in->destination = out->destination = compute_log_dir(wres, res) ;
            err->type = IO_TYPE_INHERIT ;

        }

    } else {

        {
            switch(in->type) {

                case IO_TYPE_TTY:
                case IO_TYPE_CONSOLE:
                case IO_TYPE_SYSLOG:
                case IO_TYPE_FILE:
                case IO_TYPE_S6LOG:
                case IO_TYPE_INHERIT:
                case IO_TYPE_NULL:
                case IO_TYPE_PARENT:
                case IO_TYPE_CLOSE:
                        break ;

                case IO_TYPE_NOTSET:
                    if (out->type == IO_TYPE_NOTSET || out->type == IO_TYPE_S6LOG) {
                        in->type = IO_TYPE_S6LOG ;
                        in->destination = compute_log_dir(wres, res) ;
                        break ;
                    }

                    in->type = IO_TYPE_PARENT ;
                    break ;

                default:
                    break ;
            }
        }

        if (in->type == IO_TYPE_S6LOG) {
            out->type = in->type ;
            out->destination = in->destination ;
        }

        {
            switch(out->type) {
                case IO_TYPE_TTY:
                    if (in->type == IO_TYPE_TTY)
                        out->destination = in->destination ;
                    break ;
                case IO_TYPE_FILE:
                case IO_TYPE_CONSOLE:
                case IO_TYPE_S6LOG:
                case IO_TYPE_SYSLOG:
                case IO_TYPE_INHERIT:
                    break ;

                case IO_TYPE_NULL:
                    if (in->type == IO_TYPE_NULL) {
                        out->type == IO_TYPE_INHERIT ;
                        break ;
                    }
                    break ;

                case IO_TYPE_PARENT:
                case IO_TYPE_CLOSE:
                    break ;

                case IO_TYPE_NOTSET:
                    if (in->type == IO_TYPE_TTY || in->type == IO_TYPE_S6LOG) {
                        out->type = in->type ;
                        out->destination = in->destination ;
                        break ;
                    }

                    if (in->type == IO_TYPE_NULL) {
                        out->type = IO_TYPE_INHERIT ;
                        break ;
                    }

                    if (in->type == IO_TYPE_PARENT) {
                        out->type = in->type ;
                        break ;
                    }

                    if (in->type == IO_TYPE_CLOSE) {
                        out->type = IO_TYPE_PARENT ;
                        break ;
                    }

                    out->type = in->type = IO_TYPE_S6LOG ;
                    out->destination = in->destination = compute_log_dir(wres, res) ;

                default:
                    break ;
            }
        }

        if (err->type == out->type && !strcmp(res->sa.s + in->destination, res->sa.s + out->destination))
            err->type = IO_TYPE_INHERIT ;

        if (out->type == IO_TYPE_SYSLOG) {
            err->type = out->type ;
            err->destination = out->destination ;
        }

        {
            switch(err->type) {

                case IO_TYPE_TTY:
                case IO_TYPE_FILE:
                case IO_TYPE_CONSOLE:
                case IO_TYPE_S6LOG:
                case IO_TYPE_SYSLOG:
                case IO_TYPE_INHERIT:
                case IO_TYPE_NULL:
                case IO_TYPE_PARENT:
                case IO_TYPE_CLOSE:
                    break ;
                case IO_TYPE_NOTSET:
                    err->type = IO_TYPE_INHERIT ;
                    break ;
                default:
                    break ;
            }
        }
    }

    if (res->logger.want) {
        // avoid to call parse_create_logger
        if (in->type != IO_TYPE_S6LOG && out->type != IO_TYPE_S6LOG)
            res->logger.want = 0 ;
    }


    switch (res->type) {

        case TYPE_ONESHOT:

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;

            break ;

        case TYPE_CLASSIC:

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;
            break ;

        case TYPE_MODULE:
        default:
            break ;
    }
    free(wres) ;
    return 1 ;
}
