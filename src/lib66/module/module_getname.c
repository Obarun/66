/*
 * module_getname.c
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

#include <oblibs/string.h>

int module_getname(char *module, char const *name)
{
    int r = str_contain(name, ":") ;

    if (r <= 1)
        return 0 ;

    memcpy(module, name, r - 1) ;
    module[r - 1] = 0 ;

    return 1 ;
}
