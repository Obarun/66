/*
 * 66-enable.c
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
    PROG = "66-enable" ;

    ssexec_t info = SSEXEC_ZERO ;

    info.prog = PROG ;
    info.help = help_enable ;
    info.usage = usage_enable ;

    /** The tree can be define by the frontend service file
     * with the field @intree. So, avoid to crash at call of
     * the tree_sethome() function. */
    info.skip_opt_tree = 1 ;

    return ssexec_main(argc,argv,envp,&ssexec_enable,&info) ;
}


