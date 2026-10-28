/*
 * command_outside_boot.c
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

#include <66/service.h>
#include <66/tree.h>
#include <66/command.h>

int command_outside_boot(struct resolve_hash_s const *h)
{
    int r = tree_ongroups(h->res.sa.s + h->res.path.home, h->res.sa.s + h->res.treename, TREE_GROUPS_BOOT) ;

    return r < 0 ? -1 : !r ;
}
