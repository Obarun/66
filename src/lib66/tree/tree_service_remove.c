/*
 * tree_service_remove.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdbool.h>
#include <errno.h>

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/tree.h>

int tree_service_remove(char const *base, char const *treename, char const *service)
{
    log_flow() ;

    int r = tree_contents_edit(base, treename, service, false) ;

    // a tree gone holds nothing to remove
    if (!r && errno == ENOENT)
        return 1 ;

    return r <= 0 ? -1 : 1 ;
}
