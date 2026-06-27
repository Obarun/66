/*
 * status.h
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

#ifndef SS_STATUS_H
#define SS_STATUS_H

#include <stdint.h>
#include <time.h>

#define STATUS_STATE_SIZE 49
#define STATUS_VERSION 1

enum status_state_e
{
    STATUS_STATE_DOWN = 0,
    STATUS_STATE_STARTING,
    STATUS_STATE_UP,
    STATUS_STATE_STOPPING,
    STATUS_STATE_FINISHING,
    STATUS_STATE_RESTARTING,
    STATUS_STATE_DONE,
    STATUS_STATE_FAILED
} ;
typedef enum status_state_e status_state_t ;

enum status_result_e
{
    STATUS_RESULT_SUCCESS = 0,
    STATUS_RESULT_EXITED,
    STATUS_RESULT_SIGNALED,
    STATUS_RESULT_TIMEOUT_START,
    STATUS_RESULT_TIMEOUT_STOP,
    STATUS_RESULT_CRASH_LIMIT,
    STATUS_RESULT_EXEC_FAILED
} ;
typedef enum status_result_e status_result_t ;

enum status_who_e
{
    STATUS_WHO_SELF = 0,
    STATUS_WHO_USER,
    STATUS_WHO_EVENT,
    STATUS_WHO_DEPENDENCY,
    STATUS_WHO_BOOT,
    STATUS_WHO_SHUTDOWN
} ;
typedef enum status_who_e status_who_t ;

typedef struct service_status_s service_status_t ;
struct service_status_s
{
    uint8_t version ;
    uint8_t state ;             // status_state_e
    uint8_t result ;            // status_result_e
    uint8_t who ;               // status_who_e
    uint32_t pid ;              // 0 when no process
    uint32_t code ;             // wstat / signal / raw errno, read per result
    struct timespec stamp ;        // REALTIME: entry into the current state
    struct timespec readystamp ;   // REALTIME: transition to UP
    struct timespec window_start ; // MONOTONIC: start of the current crash window
    uint8_t ndeaths ;          // deaths since window_start (crash budget)
} ;

#define STATUS_ZERO { \
    STATUS_VERSION, \
    STATUS_STATE_DOWN, \
    STATUS_RESULT_SUCCESS, \
    STATUS_WHO_SELF, \
    0, \
    0, \
    { 0, 0 }, \
    { 0, 0 }, \
    { 0, 0 }, \
    0 \
}
extern service_status_t const service_status_zero ;

extern void status_pack(char *pack, service_status_t const *st) ;
extern void status_unpack(char const *pack, service_status_t *st) ;
extern int status_write(service_status_t const *st, char const *file) ;
extern int status_read(service_status_t *st, char const *file) ;

#endif
