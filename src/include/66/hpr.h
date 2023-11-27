/*
 * hpr.h
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a modified copy of hpr.h file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#ifndef SS_HPR_H
#define SS_HPR_H

#include <stddef.h>

#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <66/constants.h>

#define INITCTL "/" SS_SCANDIR "/0/66-shutdownd/fifo"
#define INITCTL_LEN (sizeof INITCTL - 1)

#define HPR_WALL_BANNER "\n\n*** WARNING ***\nThe system is going down NOW!\n"

#define hpr_send(l,s, n) openwritenclose_unsafe(l, (s), n)
#define hpr_cancel(l) hpr_send(l,"c", 1)
extern int hpr_shutdown (char const *live, unsigned int, tain const *, unsigned int) ;
extern void hpr_wall (char const *s) ;
extern void hpr_wallv (struct iovec const *v, unsigned int n) ;

#endif
