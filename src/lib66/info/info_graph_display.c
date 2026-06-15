/*
 * info_graph_display.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/stream.h>

#include <66/info.h>

int info_graph_display(char const *name, info_graph_func *func, depth_t *depth, int last, int padding, info_graph_style *style)
{
    log_flow() ;

    int level = 1 ;

    const char *tip = "" ;

    tip = last ? style->last : style->tip ;

    while(depth->prev)
        depth = depth->prev ;

    while(depth->next)
    {
        if (!ostream_fmt(ostream_1,"%*s%-*s",style->indent * (depth->level - level) + (level == 1 ? padding : 0), "", style->indent, style->limb))
            return 0 ;

        level = depth->level + 1 ;
        depth = depth->next ;
    }

    if (!ostream_fmt(ostream_1,"%*s%*s%s", \
                level == 1 ? padding : 0,"", \
                style->indent * (depth->level - level), "", \
                tip)) return 0 ;

    int r = (*func)(name) ;
    if (!r) return 0 ;
    if (!ostream_putflush(ostream_1, "\n", 1))
        return 0 ;

    return 1 ;
}
