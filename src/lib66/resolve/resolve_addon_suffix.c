/*
 * resolve_addon_suffix.c
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

#include <stdint.h>

#include <66/resolve.h>
#include <66/constants.h>

char const *resolve_addon_suffix(uint8_t type)
{
    switch (type) {

        case DATA_SERVICE:
        case DATA_TREE:
        case DATA_TREE_MASTER: return "" ;
        case DATA_SERVICE_LIMIT: return SS_ADDON_LIMIT_SUFFIX ;
        case DATA_SERVICE_ENVIRON: return SS_ADDON_ENVIRON_SUFFIX ;
        case DATA_SERVICE_IO: return SS_ADDON_IO_SUFFIX ;
        case DATA_SERVICE_EXECUTE: return SS_ADDON_EXECUTE_SUFFIX ;
        case DATA_SERVICE_DEPENDENCIES: return SS_ADDON_DEPENDENCIES_SUFFIX ;
        case DATA_SERVICE_REGEX: return SS_ADDON_REGEX_SUFFIX ;
        case DATA_SERVICE_EVENT: return SS_ADDON_EVENT_SUFFIX ;
        default: return 0 ;
    }
}
