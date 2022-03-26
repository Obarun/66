/*
 * info_graph_display.c
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

#include <skalibs/buffer.h>
#include <skalibs/lolstdio.h>

#include <66/info.h>

int info_graph_display(char const *name, char const *obj, info_graph_func *func, depth_t *depth, int last, int padding, info_graph_style *style)
{
    log_flow() ;

    int level = 1 ;

    const char *tip = "" ;

    tip = last ? style->last : style->tip ;

    while(depth->prev)
        depth = depth->prev ;

    while(depth->next)
    {
        if (!bprintf(buffer_1,"%*s%-*s",style->indent * (depth->level - level) + (level == 1 ? padding : 0), "", style->indent, style->limb))
            return 0 ;

        level = depth->level + 1 ;
        depth = depth->next ;
    }

    if (!bprintf(buffer_1,"%*s%*s%s", \
                level == 1 ? padding : 0,"", \
                style->indent * (depth->level - level), "", \
                tip)) return 0 ;

    int r = (*func)(name, obj) ;
    if (!r) return 0 ;
    if (buffer_putsflush(buffer_1,"\n") < 0)
        return 0 ;

    return 1 ;
}
