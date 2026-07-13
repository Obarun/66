/*
 * notifier_isvalid.c
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

#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>

#include <oblibs/types.h>
#include <oblibs/log.h>

int notifier_isvalid(const char *str)
{
	uint32_t u ;

	if (!u32_scan_strict(str, &u) || u > INT_MAX)
		log_die(LOG_EXIT_USER, "invalid notification file descriptor: ", str) ;

	if (u < 3)
		log_die(LOG_EXIT_USER, "file descriptor must be 3 or more") ;

	if (fcntl(u, F_GETFD) < 0)
		log_diesys(LOG_EXIT_USER, "invalid file descriptor") ;

	return (int)u ;
}