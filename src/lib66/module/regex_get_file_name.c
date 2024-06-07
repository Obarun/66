/*
 * regex_get_file_name.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/lexer.h>
#include <oblibs/stack.h>

uint32_t regex_get_file_name(stack *filename, char const *str)
{
    log_flow() ;

    lexer_config cfg = LEXER_CONFIG_ZERO ;
    cfg.open = ":" ; cfg.olen = 1 ;
    cfg.close = ":" ; cfg.clen = 1 ;
    cfg.skip = " \t\r\n" ; cfg.skiplen = 4 ;
    cfg.kclose = cfg.kopen = 0 ;
    cfg.firstoccurence = 1 ;
    cfg.str = str ; cfg.slen = strlen(str) ;

    if (!lexer_trim(filename, &cfg))
        return 0 ;

    return ++cfg.cpos ;
}
