/*
 * instance_check.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/utils.h>

int instance_check(char const *svname)
{
    log_flow() ;

    int r ;
    r = get_len_until(svname,'@') ;
    // avoid empty value after the instance template name
    if (strlen(svname+r) <= 1 && r > 0) return 0 ;

    return r ;
}
