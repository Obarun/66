/*
 * 66-disable.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

#include <66/ssexec.h>

int main(int argc, char const *const *argv,char const *const *envp)
{
    PROG = "66-disable" ;

    ssexec_t info = SSEXEC_ZERO ;

    info.prog = PROG ;
    info.help = help_disable ;
    info.usage = usage_disable ;

    return ssexec_main(argc,argv,envp,&ssexec_disable,&info) ;
}


