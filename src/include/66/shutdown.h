/*
 * shutdown.h
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
 * The shutdown.allow access-control file: a WHITELIST of usernames permitted to
 * change the machine's power state. It is the file the poweroff/reboot/halt and
 * suspend/hibernate commands read for their `-a` check (SS_SKEL_DIR
 * "shutdown.allow"). Format: one username per line, a line whose FIRST character
 * is '#' is a comment, blank lines are ignored, entries are NOT trimmed (a line
 * must equal the username verbatim to count). Membership is the only meaning:
 * in the list = allowed.
 *
 * The file is the fixed, well-known SHUTDOWN_FILE; the functions operate on it
 * directly and share one internal load step with these properties:
 *
 *   - An ABSENT file is treated as an empty list, never as an error: search
 *     reports "not listed", list reports an empty list, add creates the file,
 *     remove is a no-op.
 *   - The file is capped at SHUTDOWN_MAX (4096) bytes: a file whose size is
 *     >= 4096 is REJECTED (the load fails with errno EFBIG).
 *   - Writes go through file_write_atomic: the new content is written to a
 *     temporary in the same directory and renamed onto the file, so the file is
 *     replaced all-or-nothing and a failure leaves any previous file untouched.
 *   - A rewrite re-emits one entry per line; blank lines present in the original
 *     are DROPPED, while comment lines and other entries are preserved.
 *
 * shutdown_isallowed() is the runtime `-a` decision and does NOT share the
 * whitelist semantics: an absent file means EVERYONE is allowed, matching the
 * historical behaviour of the shutdown access check.
 */

#ifndef SS_SHUTDOWN_H
#define SS_SHUTDOWN_H

#include <stddef.h>
#include <time.h>
#include <sys/uio.h>

#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/constants.h>

#define INITCTL SS_SCANDIR "/0/66-shutdownd/fifo"
#define INITCTL_LEN (sizeof INITCTL - 1)
#define SHUTDOWN_FILE SS_SKEL_DIR "shutdown.allow"

#define HPR_WALL_BANNER "\n\n*** WARNING ***\nThe system is going down NOW!\n"

#define hpr_send(l,s, n) file_write(l, (s), n)
#define hpr_cancel(l) hpr_send(l,"c", 1)
extern int hpr_shutdown (char const *live, unsigned int, struct timespec const *, unsigned int) ;
extern void hpr_wall (char const *s) ;
extern void hpr_wallv (struct iovec const *v, unsigned int n) ;
extern int umountall (void) ;

/**
 * @brief Is @user listed in SHUTDOWN_FILE?
 * @param[in] user Username to look up. An invalid bare username (NULL, empty, or
 *                 containing whitespace or '#') can never appear in the file, so
 *                 it is reported "not listed" rather than as an error.
 * @return 1 if @user is present as a verbatim line.
 * @return 0 if @user is not listed, the file is absent, or @user is not a valid
 *         bare username. errno is NOT set on any of these 0 paths.
 * @return -1 on a load error (errno set: EFBIG if the file is >= SHUTDOWN_MAX
 *         (4096) bytes; otherwise the errno left by the underlying stat /
 *         strbuf_read_file, or by the line-splitting step).
 */
extern int shutdown_search (char const *user) ;

/**
 * @brief Add @user to SHUTDOWN_FILE (idempotent).
 *
 * Loads the file (an absent file is an empty list), and if @user is already
 * listed returns success WITHOUT rewriting. Otherwise appends @user and writes
 * the whole file back atomically, creating it if absent and preserving existing
 * comments and entries; blank lines in the original are dropped by the rewrite.
 *
 * @param[in] user Username to add. Must be a valid bare username.
 * @return 1 on success, including the idempotent no-op when @user is already listed.
 * @return 0 on error, errno set (EINVAL if @user is not a valid bare username;
 *         EFBIG / load errno on a load failure; the strbuf errno when staging the
 *         new content fails; the file_write_atomic errno when the write fails).
 */
extern int shutdown_add (char const *user) ;

/**
 * @brief Remove @user from SHUTDOWN_FILE (idempotent).
 *
 * Loads the file (an absent file is an empty list), and if @user is not listed
 * returns success WITHOUT rewriting. Otherwise drops the matching entry and
 * writes the whole file back atomically — when removal empties the list it writes
 * an empty file. Comment lines and other entries are preserved; blank lines in
 * the original are dropped by the rewrite.
 *
 * @param[in] user Username to remove. Must be a valid bare username.
 * @return 1 on success, including the idempotent no-op when @user is not listed.
 * @return 0 on error, errno set (EINVAL if @user is not a valid bare username;
 *         EFBIG / load errno on a load failure; the strbuf errno when staging the
 *         new content fails; the file_write_atomic errno when the write fails).
 */
extern int shutdown_remove (char const *user) ;

/**
 * @brief Append the usernames listed in SHUTDOWN_FILE, one per line, to @out.
 *
 * Loads the file (an absent file yields nothing appended) and appends every valid
 * bare username, each followed by a '\n'. Comment lines, blank lines and any
 * malformed entry are skipped. @out is APPENDED to, not reset.
 *
 * @param[in,out] out  Destination buffer; receives the usernames (with trailing
 *                     newlines) appended to whatever it already holds.
 * @return 1 on success.
 * @return 0 on error, errno set (EFBIG / load errno on a load failure; the strbuf
 *         errno when growing @out fails — @out may then hold a partial list).
 */
extern int shutdown_list (strbuf *out) ;

/**
 * @brief Runtime access gate for a power-state change: may it proceed now?
 *
 * Unlike the whitelist editing functions, an ABSENT file means everyone is
 * allowed (historical `-a` behaviour). When the file exists, the change is
 * allowed only if a user listed in it is currently logged in (per utmp).
 *
 * @return 1 if allowed (file absent, or a listed user is logged in).
 * @return 0 if denied (file present and no listed user is currently logged in).
 * @return -1 on a load error (errno set: EFBIG if the file is >= SHUTDOWN_MAX
 *         (4096) bytes; otherwise the errno left by the underlying stat /
 *         strbuf_read_file / line-splitting step).
 */
extern int shutdown_isallowed (void) ;

#endif
