/*
 * 66-all.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
    PROG = "66-all" ;

    ssexec_t info = SSEXEC_ZERO ;

    info.prog = PROG ;
    info.help = help_all ;
    info.usage = usage_all ;

    /** 66-all supports to not define a default tree to start/stop,
     * but ssexec_main do not support it. The tree_sethome() function
     * will complain if a current tree is not define.
     * So, ask to not set it. */

    info.skip_opt_tree = 1 ;

    return ssexec_main(argc,argv,envp,&ssexec_all,&info) ;
}
