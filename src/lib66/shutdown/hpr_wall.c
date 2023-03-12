/*
 * hpr_wall.c
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
 *
 * This file is a strict copy of hpr_wall.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <string.h>
#include <sys/uio.h>

#include "66/hpr.h"

void hpr_wall (char const *s)
{
  struct iovec v[2] = { { .iov_base = (char *)s, .iov_len = strlen(s) }, { .iov_base = "\n", .iov_len = 1 } } ;
  hpr_wallv(v, 2) ;
}
