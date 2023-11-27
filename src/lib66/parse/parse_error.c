/*
 * parse_error.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <66/enum.h>

void parse_error(int ierr, int idsec, int idkey)
{
    log_flow() ;

    char const *section = get_key_by_enum(ENUM_SECTION, idsec) ;
    /* start stop enum are the same, enum_all must increase by one to match
     * the correct list
     * */
    char const *key = get_key_by_enum(idsec < 2 ? idsec + 1 : idsec,idkey) ;

    switch(ierr)
    {
        case 0:
            log_warn("invalid value for key: ",key,": in section: ",section) ;
            break ;
        case 1:
            log_warn("multiple definition of key: ",key,": in section: ",section) ;
            break ;
        case 2:
            log_warn("same value for key: ",key,": in section: ",section) ;
            break ;
        case 3:
            log_warn("key: ",key,": must be an integrer value in section: ",section) ;
            break ;
        case 4:
            log_warn("key: ",key,": must be an absolute path in section: ",section) ;
            break ;
        case 5:
            log_warn("key: ",key,": must be set in section: ",section) ;
            break ;
        case 6:
            log_warn("invalid format of key: ",key,": in section: ",section) ;
            break ;
        case 7:
            log_warnu("parse key: ",key,": in section: ",section) ;
            break ;
        case 8:
            log_warnu("clean value of key: ",key,": in section: ",section) ;
            break ;
        case 9:
            log_warn("empty value of key: ",key,": in section: ",section) ;
            break ;
        default:
            log_warn("unknown parse_err number") ;
            break ;
    }
}
