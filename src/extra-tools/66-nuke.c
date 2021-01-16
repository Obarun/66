/*
 * 66-nuke.c
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
 * This file is a strict copy of s6-echo.c file
 * coming from skarnet software at https://skarnet.org/software/s6-portable-utils.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <signal.h>

int main (void)
{
  return kill(-1, SIGKILL) < 0 ;
}
