/*
 * info_graph_init.c
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
 * */

#include <oblibs/log.h>

#include <66/info.h>

unsigned int MAXDEPTH = 1 ;

info_graph_style graph_utf8 = {
    UTF_VR UTF_H,
    UTF_UR UTF_H,
    UTF_V " ",
    2
} ;

info_graph_style graph_default = {
    "|-", // tip
    "`-", // last
    "|", // limb
    2
} ;

depth_t info_graph_init(void)
{
    log_flow() ;

    depth_t d = {
        NULL,
        NULL,
        1
    } ;

    return d ;
}
