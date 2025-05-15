/*
 * caps.c
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

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/capability.h>

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>
#include <oblibs/bits.h>

#include <66/service.h>
#include <66/caps.h>
#include <66/enum.h>

void parse_store_caps(stack *result, stack *store, uint32_t *ncaps)
{
    size_t pos = 0 ;
    bool todel = false, has_dflags = false ;
    ssize_t tflag ;
    uint32_t cap = 0 ;
    resolve_enum_table_t table = E_TABLE_PARSER_CAPS_ZERO ;

    char *t = 0 ;

    bitset_t aflags = bitset_create_empty(CAPS_MYLAST_CAP) ;
    bitset_t dflags = bitset_create_empty(CAPS_MYLAST_CAP) ;

    FOREACH_STK(store, pos) {

        todel = false ;
        t = store->s + pos ;
        if (store->s[pos] == '!') {
            todel = true ;
            t++ ;
        }

        tflag = key_to_enum(table.u.parser.list, t) ;
        if (tflag < 0) {
            log_die(LOG_EXIT_SYS, "unknown capability flag: ", t) ;
        } else if (tflag >= CAPS_MYLAST_CAP) {
            log_die(LOG_EXIT_SYS, "capability out of range: ", t);
        }

        bitset_set(&aflags, (uint32_t)tflag) ;

        if (todel) {
            has_dflags = true ;
            bitset_set(&dflags, (uint32_t)tflag) ;
        }
    }

    if (has_dflags) {

        for (; cap < CAPS_MYLAST_CAP ; cap++) {

            if (bitset_isvalid(&dflags, cap))
                 continue ;

            if (!stack_add_g(result, *table.u.parser.list[cap].name))
                log_die_nomem("stack") ;
        }

    } else {

        for (; cap < CAPS_MYLAST_CAP ; cap++) {

            if (bitset_isvalid(&aflags, cap))
                if (!stack_add_g(result, *table.u.parser.list[cap].name))
                    log_die_nomem("stack") ;
        }
    }

    if (result->len) {
        (*ncaps) = result->count ;
        if (!stack_string_rebuild_with_delim(result, ' '))
            log_dieusys(LOG_EXIT_SYS, "rebuild stack") ;
    }
}

static int cap_set(cap_user_header_t header, const cap_user_data_t data)
{
    return syscall(SYS_capset, header, data) ;
}

static int cap_get(cap_user_header_t header, cap_user_data_t data)
{
    return syscall(SYS_capget, header, data) ;
}

/*
static int get_bset(bitset_t *bset)
{
    struct __user_cap_header_struct header = { .version = _LINUX_CAPABILITY_VERSION_3, .pid = 0 };
    struct __user_cap_data_struct data[2] = { {0}, {0} };
    uint32_t pos = 0;

    if (cap_get(&header, data) < 0)
        return 0;

    for (pos = 0; pos < CAPS_MYLAST_CAP; pos++) {
        uint32_t word = pos / 32;
        uint32_t bit = pos % 32;
        if (data[word].inheritable & (1U << bit)) { // Bounding set is part of inheritable in some contexts
            bitset_set(bset, pos);
        } else {
            bitset_clear(bset, pos);
        }
    }

    return 1;
}
*/

/** This function makes a separate prctl syscall
 * for each capability.
 * Fine, we only deal with 41 caps at the moment.
 * The commented function above can be a better implementation
 * at some point if deeper configuration is needed.
 * KISS for now keeping the following declaration for
 * get_bset().
 * */
static int get_bset(bitset_t *bset)
{
    uint32_t pos = 0 ;

    for (; pos < CAPS_MYLAST_CAP ; pos++) {

        int ret = prctl(PR_CAPBSET_READ, pos, 0, 0, 0) ;
        if (ret < 0)
            return 0 ;

        if (ret) {
            bitset_set(bset, pos) ;
        } else {
            bitset_clear(bset, pos) ;
        }
    }

    return 1 ;
}

static void string_to_bitset(bitset_t *bset, const char *s)
{
    _alloc_stk_(stk, strlen(s)) ;
    size_t pos = 0 ;
    ssize_t flag ;
    resolve_enum_table_t table = E_TABLE_PARSER_CAPS_ZERO ;

    if (!stack_string_clean(&stk, s))
        log_dieusys(LOG_EXIT_SYS, "clean string") ;

    FOREACH_STK(&stk, pos) {
        flag = key_to_enum(table.u.parser.list, stk.s + pos) ;
        if (flag < 0)
            log_die(LOG_EXIT_SYS, "unknown capability flag: ", stk.s + pos) ;

        bitset_set(bset, (uint32_t)flag) ;
    }
}

// Configure bounding set and thread capabilities
static void execute_caps_bound(bitset_t *caps)
{
    struct __user_cap_header_struct header = { .version = _LINUX_CAPABILITY_VERSION_3, .pid = 0 } ;
    struct __user_cap_data_struct data[2] = { {0}, {0} } ;
    uint32_t pos = 0 ;
    bool ncaps = false ;

    if (prctl(PR_CAPBSET_READ, CAP_SETPCAP, 0, 0, 0) <= 0)
        log_die(LOG_EXIT_SYS, "CAP_SETPCAP is not in the bounding set of the process") ;

    if (cap_get(&header, data) < 0)
        log_dieusys(LOG_EXIT_SYS, "retrieve actual capabilities") ;

    data[0].permitted |= (1U << CAP_SETPCAP) ;
    data[0].effective |= (1U << CAP_SETPCAP) ;

    // Clear all other thread capabilities
    if (cap_set(&header, data) < 0)
        log_dieusys(LOG_EXIT_SYS, "clear capabilities") ;

    data[0] = (struct __user_cap_data_struct){0} ;
    data[1] = (struct __user_cap_data_struct){0} ;

    // Build capability bitmasks
    for (; pos < CAPS_MYLAST_CAP ; pos++) {
        if (bitset_isvalid(caps, pos) || pos == CAP_SETPCAP) {
            ncaps = true ;
            uint32_t word = pos / 32 ;
            uint32_t bit = pos % 32 ;
            data[word].permitted |= (1U << bit) ;
            data[word].effective |= (1U << bit) ;
            data[word].inheritable |= (1U << bit) ;
        }
    }

    // Set thread capabilities
    if (ncaps) {

        if (cap_set(&header, data) < 0)
            log_dieusys(LOG_EXIT_SYS, "set capabilities") ;
    }

    // Drop unneeded bounding set capabilities
    pos = 0 ;
    for (; pos < CAPS_MYLAST_CAP ; pos++) {

        if (!bitset_isvalid(caps, pos)) {

            if (prctl(PR_CAPBSET_DROP, pos, 0, 0, 0) < 0)
                log_dieusys(LOG_EXIT_SYS, "drop capabilities") ;
        }
    }
}

// Configure ambient capabilities
static void execute_caps_ambient(resolve_service_t *res)
{
    bitset_t capsbound = bitset_create_empty(CAPS_MYLAST_CAP) ;
    bitset_t capsambient = bitset_create_empty(CAPS_MYLAST_CAP) ;
    bitset_t bset = bitset_create_empty(CAPS_MYLAST_CAP) ;
    bitset_t *pbset = 0 ;
    uint32_t pos = 0 ;

    if (res->execute.capsbound)
        string_to_bitset(&capsbound, res->sa.s + res->execute.capsbound) ;

    string_to_bitset(&capsambient, res->sa.s + res->execute.capsambient) ;

    if (!res->owner && res->execute.capsbound) {

        pbset = &capsbound ;

    } else {

        if (!get_bset(&bset))
            log_dieusys(LOG_EXIT_SYS, "get current bounding set") ;

        pbset = &bset ;

    }

    execute_caps_bound(pbset) ;

    for (; pos < CAPS_MYLAST_CAP ; pos++) {

        if (bitset_isvalid(&capsambient, pos)) {

            if (!bitset_isvalid(pbset, pos)) {
                log_warn("ambient capability ", enum_str_parser_caps[pos], " not in bounding set -- ignoring it") ;
                continue ;
            }

            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, pos, 0, 0) < 0)
                log_diesys(LOG_EXIT_SYS, "PR_CAP_AMBIENT_RAISE failed for capability: ", enum_str_parser_caps[pos]) ;

        }
    }
}

void execute_caps(resolve_service_t *res)
{
    if (res->execute.capsbound && !res->owner) {
        bitset_t c = bitset_create_empty(CAPS_MYLAST_CAP) ;
        string_to_bitset(&c, res->sa.s + res->execute.capsbound) ;
        execute_caps_bound(&c) ;
    }

    if (res->execute.capsambient)
        execute_caps_ambient(res) ;
}