/*
 * backup.h
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_BACKUP_H
#define SS_BACKUP_H

#include <skalibs/stralloc.h>

#include <66/ssexec.h>

extern int backup_make_new(ssexec_t *info,unsigned int type) ;
extern int backup_cmd_switcher(unsigned int verbosity,char const *cmd, ssexec_t *info) ;
extern int backup_switcher(int argc, char const *const *argv,ssexec_t *info) ;
extern int backup_realpath_sym(stralloc *sa,ssexec_t *info,unsigned int type) ;

#endif
