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
#include <string.h> // strcmp

#define STATUS_STATE_SIZE 49
#define STATUS_VERSION 1

/* The status vocabulary lives here, as a single source of truth: the enums and
 * their canonical words are one table each, so every consumer (66 status, the
 * resolve display, the service graph, the event parser) translates the same way
 * -- no per-site divergence. Callers add their own decoration (colour, the exit
 * code, "by <who>", ...); only the base word comes from here. */

#define STATUS_STATE_TABLE(macro) \
    macro(STATUS_STATE_DOWN,       "down") \
    macro(STATUS_STATE_STARTING,   "starting") \
    macro(STATUS_STATE_UP,         "up") \
    macro(STATUS_STATE_STOPPING,   "stopping") \
    macro(STATUS_STATE_FINISHING,  "finishing") \
    macro(STATUS_STATE_RESTARTING, "restarting") \
    macro(STATUS_STATE_DONE,       "done") \
    macro(STATUS_STATE_FAILED,     "failed")

typedef enum status_state_e status_state_t ;
enum status_state_e
{
#define STATUS_STATE_ENUM(id, str) id,
    STATUS_STATE_TABLE(STATUS_STATE_ENUM)
#undef STATUS_STATE_ENUM
    STATUS_STATE_ENDOFKEY
} ;

#define STATUS_RESULT_TABLE(macro) \
    macro(STATUS_RESULT_SUCCESS,       "success") \
    macro(STATUS_RESULT_EXITED,        "exited") \
    macro(STATUS_RESULT_SIGNALED,      "signaled") \
    macro(STATUS_RESULT_TIMEOUT_START, "timeout-start") \
    macro(STATUS_RESULT_TIMEOUT_STOP,  "timeout-stop") \
    macro(STATUS_RESULT_CRASH_LIMIT,   "crash-limit") \
    macro(STATUS_RESULT_EXEC_FAILED,   "exec-failed")

typedef enum status_result_e status_result_t ;
enum status_result_e
{
#define STATUS_RESULT_ENUM(id, str) id,
    STATUS_RESULT_TABLE(STATUS_RESULT_ENUM)
#undef STATUS_RESULT_ENUM
    STATUS_RESULT_ENDOFKEY
} ;

#define STATUS_WHO_TABLE(macro) \
    macro(STATUS_WHO_SELF,       "self") \
    macro(STATUS_WHO_USER,       "user") \
    macro(STATUS_WHO_EVENT,      "event") \
    macro(STATUS_WHO_DEPENDENCY, "dependency") \
    macro(STATUS_WHO_BOOT,       "boot") \
    macro(STATUS_WHO_SHUTDOWN,   "shutdown")

typedef enum status_who_e status_who_t ;
enum status_who_e
{
#define STATUS_WHO_ENUM(id, str) id,
    STATUS_WHO_TABLE(STATUS_WHO_ENUM)
#undef STATUS_WHO_ENUM
    STATUS_WHO_ENDOFKEY
} ;

/** @return the canonical word of a state, or "unknown" if out of range. */
static inline char const *status_state_to_string(status_state_t v)
{
    switch (v) {
#define STATUS_STATE_CASE(id, str) case id: return str ;
        STATUS_STATE_TABLE(STATUS_STATE_CASE)
#undef STATUS_STATE_CASE
        default: return "unknown" ;
    }
}

/** @return the state id for @p s, or -1 if it is not a known state word. */
static inline int status_state_from_string(char const *s)
{
#define STATUS_STATE_IF(id, str) if (!strcmp(s, str)) return id ;
    STATUS_STATE_TABLE(STATUS_STATE_IF)
#undef STATUS_STATE_IF
    return -1 ;
}

/** @return the canonical word of a result, or "unknown" if out of range. */
static inline char const *status_result_to_string(status_result_t v)
{
    switch (v) {
#define STATUS_RESULT_CASE(id, str) case id: return str ;
        STATUS_RESULT_TABLE(STATUS_RESULT_CASE)
#undef STATUS_RESULT_CASE
        default: return "unknown" ;
    }
}

/** @return the result id for @p s, or -1 if it is not a known result word. */
static inline int status_result_from_string(char const *s)
{
#define STATUS_RESULT_IF(id, str) if (!strcmp(s, str)) return id ;
    STATUS_RESULT_TABLE(STATUS_RESULT_IF)
#undef STATUS_RESULT_IF
    return -1 ;
}

/** @return the canonical word of a who, or "unknown" if out of range. */
static inline char const *status_who_to_string(status_who_t v)
{
    switch (v) {
#define STATUS_WHO_CASE(id, str) case id: return str ;
        STATUS_WHO_TABLE(STATUS_WHO_CASE)
#undef STATUS_WHO_CASE
        default: return "unknown" ;
    }
}

/** @return the who id for @p s, or -1 if it is not a known who word. */
static inline int status_who_from_string(char const *s)
{
#define STATUS_WHO_IF(id, str) if (!strcmp(s, str)) return id ;
    STATUS_WHO_TABLE(STATUS_WHO_IF)
#undef STATUS_WHO_IF
    return -1 ;
}

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
