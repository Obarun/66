/*
 * tree_seed_file_isvalid.c
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
#include <sys/types.h>

#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/tree.h>

/** @Return -1 bad format e.g want REG get DIR
 * @Return  0 fail
 * @Return success */
int tree_seed_file_isvalid(char const *seedpath, char const *treename)
{
    log_flow() ;

    int r ;
    size_t slen = strlen(seedpath), tlen = strlen(treename) ;
    char seed[slen + tlen + 1] ;
    auto_strings(seed, seedpath, treename) ;

    r = scan_mode(seed, S_IFREG) ;

    return r ;
}
