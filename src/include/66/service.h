/*
 * service.h
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

#ifndef SS_SERVICE_H
#define SS_SERVICE_H

#include <skalibs/stralloc.h>

int service_isenabled(char const *sv) ;
int service_isenabledat(stralloc *tree, char const *sv) ;

#endif
