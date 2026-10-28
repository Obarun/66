/*
 * command_refuse_member.c
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

#include <oblibs/log.h>

#include <66/ssexec.h>
#include <66/module.h>
#include <66/command.h>

void command_refuse_member(char const *const *argv, int argc, char const *verb)
{
    log_flow() ;

    for (int i = 0 ; i < argc ; i++) {

        char module[strlen(argv[i]) + 1] ;

        if (argv[i][0] == '/' || !module_getname(module, argv[i]))
            continue ;

        log_die(LOG_EXIT_USER, SSEXEC_MSG_MEMBER(argv[i], module, verb)) ;
    }
}
