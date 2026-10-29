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
    size_t r = str_search(name, ':') ;

    if (!name[r] || !r)
        return 0 ;

    memcpy(module, name, r) ;
    module[r] = 0 ;

    return 1 ;
}
