/*
 * scandir.h
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
 *
 * Data-structure substrate for the 66-scandir port of s6-svscan, private to the
 * binary:
 *
 *   - svpool_t : a free-list index allocator with delete-safe iteration
 *                (replaces skalibs genset). Its occupancy is an oblibs bits.h
 *                bitset32_t (inline value, no allocation). The bit-set families
 *                themselves (active/tmpactive) are bitset32_t directly in the
 *                binary -- no bespoke bitmap type is needed because 66's service
 *                count cap (SS_MAX_SERVICE) is a compile-time constant well under
 *                bitset32_t's 1024-bit capacity.
 *   - devino_t : the (dev,ino) identity key for the by_devino fhash_cb index.
 *
 * The two inverse indexes (PID->slot, (dev,ino)->slot) are oblibs fhash_cb tables,
 * declared directly in the binary with their typed callbacks.
 */

#ifndef SS_SCANDIR_H
#define SS_SCANDIR_H

#include <stdint.h>
#include <sys/types.h>

#include <oblibs/bits.h>

/**
 * @struct svpool_s
 * @brief A free-list index allocator over caller-owned storage, with delete-safe
 *        iteration.
 *
 * Hands out and reclaims integer indices in the range `[0, n)`, where `n` is the
 * capacity fixed at `svpool_init()`. It performs no allocation of its own: the
 * caller supplies the `freelist` backing array, and the occupancy `bitset32_t`
 * is an inline value embedded in the structure. `svpool_new()` pops the lowest
 * free index and `svpool_delete()` pushes it back, both in O(1); the occupancy
 * bitset drives `svpool_iter()`, which walks the live indices by storage position
 * and tolerates the iterator deleting the index it is visiting. This replaces the
 * skalibs `genset` the s6-svscan port used for its services pool.
 *
 * The structure is created empty by `SVPOOL_ZERO` and must be passed to
 * `svpool_init()` before any other operation; the field invariants below hold
 * only after a successful init.
 *
 * @param freelist
 * Caller-owned array of at least `n` `uint32_t` entries, used as a stack of the
 * currently-free indices. Entries `freelist[0 .. freetop-1]` are the free
 * indices; the rest is unused scratch. The pool does not own this storage and
 * never frees it; it must outlive the pool.
 *
 * @param used
 * Inline occupancy bitset (an oblibs `bitset32_t` value, no heap). Bit `i` is set
 * while index `i` is allocated. Because `bitset32_t` holds at most
 * `UINT32_BITS_MAX` (1024) bits, this caps the pool's `n` at 1024; a larger `n`
 * is rejected by `svpool_init()`.
 *
 * @param n
 * The pool capacity: the number of distinct indices, all in `[0, n)`. Fixed at
 * `svpool_init()` and never changed afterwards.
 *
 * @param freetop
 * The number of free indices currently available, i.e. the count of valid entries
 * in `freelist[0 .. freetop-1]`. Equals `n` just after init (all free) and 0 when
 * the pool is full. Decremented by `svpool_new()`, incremented by
 * `svpool_delete()`.
 *
 * @param high
 * A high-water mark: one past the highest index ever returned by `svpool_new()`
 * (0 when nothing has been allocated yet). It is the exclusive upper bound
 * `svpool_iter()` scans, and only ever grows; `svpool_delete()` does not lower it.
 */
typedef struct svpool_s svpool_t ;
struct svpool_s
{
    uint32_t *freelist ; // caller storage, n entries (a stack of free indices)
    bitset32_t used ; // occupancy (inline value type; caps n at UINT32_BITS_MAX = 1024)
    uint32_t n ; // capacity
    uint32_t freetop ; // number of free indices available (freelist[0..freetop-1])
    uint32_t high ; // 1 + highest index ever allocated (iteration upper bound)
} ;

/**
 * @brief Static initialiser for an empty, not-yet-usable `svpool_t`.
 *
 * Expands to a brace initialiser that nulls `freelist`, zeroes `used` (via
 * `BITSET32_ZERO`), and sets `n`, `freetop` and `high` to 0. A pool initialised
 * this way has capacity 0 and must be handed to `svpool_init()` before any
 * `svpool_new()`, `svpool_delete()` or `svpool_iter()` call.
 */
#define SVPOOL_ZERO { 0, BITSET32_ZERO, 0, 0, 0 }

/**
 * @struct devino_s
 * @brief The `(device, inode)` identity key of a watched directory.
 *
 * Holds the two `stat(2)` fields that together identify a filesystem object
 * uniquely across mount points: the device id and the inode number. It is the
 * key type of 66-scandir's second inverse index, the `(dev,ino) -> slot`
 * `fhash_cb` table (`by_devino`), used to recognise a service directory already
 * being supervised regardless of the path it is reached by.
 *
 * Equality is performed by the index's own `fhash_cb` callback in the binary
 * (`bydevino_eq` in 66-scandir.c), which compares the named fields `dev` and
 * `ino` directly — never the raw struct bytes — so it is immune to any padding
 * between or after the fields. There is intentionally no equality (or hash)
 * function for `devino_t` in this header; the table owns that logic.
 *
 * @param dev
 * The device id (`st_dev`) of the directory.
 *
 * @param ino
 * The inode number (`st_ino`) of the directory.
 */
typedef struct devino_s devino_t ;
struct devino_s
{
    dev_t dev ;
    ino_t ino ;
} ;

/**
 * @brief Initialise @p as an index allocator of capacity @n over @freelist.
 *
 * Binds @p to the caller-owned @freelist (which must have room for at least @n
 * `uint32_t` entries) and sizes the inline occupancy bitset to @n bits. On
 * success the pool starts with every index in `[0, @n)` free; the freelist is
 * primed in descending order so that the first `svpool_new()` calls hand out
 * 0, 1, 2, … in turn, keeping the high-water mark tight. @p need not be
 * initialised beforehand; this function writes all of its fields.
 *
 * @param[out] p        Pool to initialise. Must not be NULL.
 * @param[in]  freelist Caller-owned array of at least @n `uint32_t` entries. Must
 *                      not be NULL. Not owned by the pool; must outlive it. Its
 *                      contents are overwritten.
 * @param[in]  n        Capacity, i.e. the number of indices `[0, @n)`. Must be
 *                      `> 0` and `<= UINT32_BITS_MAX` (1024); see @return.
 *
 * @return 1 on success.
 * @return 0 on failure (errno set to `EINVAL`).
 *
 * @retval 0 errno is set to `EINVAL` if @p is NULL, @freelist is NULL, or @n is 0.
 * @retval 0 errno is set to `EINVAL` if @n exceeds the `bitset32_t` capacity
 *         `UINT32_BITS_MAX` (1024): `bitset32_init(@n)` then yields a bitset with
 *         `size == 0`, which is rejected.
 *
 * @note Failure leaves @p unspecified: on the capacity-rejection path `used` has
 *       already been overwritten with the zeroed (`size == 0`) bitset, while
 *       `freelist`, `n`, `freetop` and `high` are not set. Do not use @p after a
 *       failed init; re-init it or discard it.
 */
extern int svpool_init(svpool_t *p, uint32_t *freelist, uint32_t n) ;

/**
 * @brief Allocate and return the lowest free index of the pool.
 *
 * Pops the lowest currently-free index, marks it occupied in the bitset, and
 * advances the high-water mark if needed. Combined with the descending freelist
 * primed by `svpool_init()`, consecutive calls on a fresh pool yield 0, 1, 2, …
 *
 * @param[in,out] p Pool to allocate from. Must not be NULL and must have been
 *                  successfully initialised by `svpool_init()`.
 *
 * @return An index in `[0, n)` on success: the lowest index that was free.
 * @return @p->n (i.e. the capacity, an out-of-range sentinel) when the pool is
 *         full (no free index, `freetop == 0`). No bit is set and no state changes
 *         in that case. errno is not set.
 *
 * @note The caller must test the return against @p->n to detect exhaustion; the
 *       function does not use errno to report a full pool.
 */
extern uint32_t svpool_new(svpool_t *p) ;

/**
 * @brief Free the index @i, returning it to the pool.
 *
 * Clears @i's occupancy bit and pushes it back onto the freelist, so a later
 * `svpool_new()` may hand it out again. The high-water mark is left unchanged.
 *
 * @param[in,out] p Pool to free into. Must not be NULL and must have been
 *                  successfully initialised by `svpool_init()`.
 * @param[in]     i Index to free.
 *
 * @return Nothing.
 *
 * @note This is a no-op (no state change) when @i is out of range (`@i >= @p->n`)
 *       or when @i is not currently allocated. The latter is a deliberate
 *       double-free guard: freeing an already-free index does nothing rather than
 *       corrupting the freelist.
 */
extern void svpool_delete(svpool_t *p, uint32_t i) ;

/**
 * @brief Invoke @f on every live index, in ascending order, until @f stops it.
 *
 * Scans indices from 0 up to (but excluding) the high-water mark @p->high, and
 * for each one currently allocated calls `@f(i, @aux)`. If @f returns 0 the
 * iteration stops immediately; any non-zero return continues to the next live
 * index. Indices that were never allocated, and those above the high-water mark,
 * are skipped. This mirrors what s6-svscan's `scan()` does through
 * `genset_iter` + `remove_deadinactive_iter`.
 *
 * @param[in,out] p   Pool to iterate. Must not be NULL and must have been
 *                    successfully initialised by `svpool_init()`.
 * @param[in]     f   Callback applied to each live index. Must not be NULL.
 *                    Receives the index and @aux; returns 0 to stop the iteration
 *                    or non-zero to continue. See @note for what it may and may
 *                    not do.
 * @param[in]     aux Opaque pointer passed through unchanged to every call of @f.
 *
 * @return Nothing.
 *
 * @note @f MAY call `svpool_delete()` on the current index, or on any already-
 *       visited or not-yet-reached index, without disturbing the walk: the scan
 *       is driven by storage position and re-reads the occupancy bit at each step.
 *       @f MUST NOT call `svpool_new()` during the iteration: allocating a new
 *       index can raise the high-water mark and set a bit the in-progress scan has
 *       not yet passed, which would re-enter @f for that index within the same
 *       walk.
 */
extern void svpool_iter(svpool_t *p, int (*f)(uint32_t i, void *aux), void *aux) ;

#endif
