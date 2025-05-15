/*
 * enum_struct.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_ENUM_STRUCT_H
#define SS_ENUM_STRUCT_H

// this is avoid circular dependencies between enum.h and enum_xxx.h
typedef struct key_description_s key_description_t ;
struct key_description_s
{
    const char *const *name ;
    int const id ;
    int const expected ;
} ;

#endif