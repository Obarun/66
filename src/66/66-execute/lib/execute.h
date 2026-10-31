/*
 * execute.h
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

#ifndef SS_EXECUTE_H
#define SS_EXECUTE_H

#include <stddef.h>

#include <oblibs/strbuf.h>

/**
 * @brief Split an execline script text into words, exactly as execlineb does.
 *
 * Byte for byte the result of execline's el_parse: quoting, escapes, comments
 * and blocks, a block word being prefixed with one space per nesting level.
 * The words are appended to @sa, each terminated by a NUL; what @sa held before
 * is kept. Parsing stops at @len or at the first NUL byte.
 *
 * @param[in,out] sa  Receives the words.
 * @param[in] s       The script text.
 * @param[in] len     Length of @s.
 *
 * @return the number of words (0 for a text without any).
 * @return -1 on a system error, errno set (ENOMEM, E2BIG).
 * @return -2 on a syntax error.
 * @return -3 on a '{' never closed.
 * @return -4 on a '}' without its '{'.
 * On every negative return @sa is left as it was on entry.
 */
extern int execute_parse(strbuf *sa, char const *s, size_t len) ;

#endif
