/*
 * state.h
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

#ifndef SS_STATE_H
#define SS_STATE_H

#include <stddef.h>

#include <skalibs/uint32.h>

#include <66/service.h>

#define STATE_STATE_SIZE 32

#define STATE_FLAGS_FALSE (1 << 1) // 2
#define STATE_FLAGS_TRUE (1 << 2) // 4
#define STATE_FLAGS_UNKNOWN (1 << 3) // 8

#define STATE_FLAGS_TOINIT (1 << 4) // 16
#define STATE_FLAGS_TORELOAD (1 << 5) // ...
#define STATE_FLAGS_TORESTART (1 << 6)
#define STATE_FLAGS_TOUNSUPERVISE (1 << 7)
#define STATE_FLAGS_TOPARSE (1 << 8)
#define STATE_FLAGS_ISPARSED (1 << 9)
#define STATE_FLAGS_ISSUPERVISED (1 << 10)
#define STATE_FLAGS_ISUP (1 << 11)

#define STATE_FLAGS_TOPROPAGATE (1 << 12)
#define STATE_FLAGS_WANTUP (1 << 13)
#define STATE_FLAGS_WANTDOWN (1 << 14)

#define STATE_FLAGS_ISEARLIER (1 << 15)

typedef struct ss_state_s ss_state_t, *ss_state_t_ref ;
struct ss_state_s
{
    /** STATE_FLAGS_FALSE -> no,
     * STATE_FLAGS_TRUE -> yes
     * for all field */
    uint32_t toinit ; // runtime, in other case sanitize_init look for the presence of live.statedir
    uint32_t toreload ; // runtime
    uint32_t torestart ; // runtime
    uint32_t tounsupervise ; // runtime
    uint32_t toparse ; // useless
    uint32_t isparsed ; // doesn't change but used a lot, can be set to STATE_FLAGS_TRUE at initialization of the state struct
    uint32_t issupervised ; // set in sanitize_init, so runtime
    uint32_t isup ; // runtime
} ;

#define STATE_ZERO { \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
    STATE_FLAGS_FALSE, \
}
extern ss_state_t const ss_state_zero ;

extern void state_rmfile(resolve_service_t *res) ;
extern void state_pack(char *base, ss_state_t *sta) ;
extern void state_unpack(char *pack, ss_state_t *sta) ;
extern void state_set_flag(ss_state_t *sta, int flags,int flags_val) ;
extern int state_check(resolve_service_t *res) ;
extern int state_write(ss_state_t *sta, resolve_service_t *res) ;
extern int state_write_remote(ss_state_t *sta, char const *tmp) ;
extern int state_read(ss_state_t *sta, resolve_service_t *res) ;
extern int state_messenger(resolve_service_t *res, uint32_t flag, uint32_t value) ;

#endif
