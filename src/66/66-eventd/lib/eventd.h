/*
 * eventd.h
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

#ifndef SS_EVENTD_H
#define SS_EVENTD_H

#include <stdint.h>

#include <66/event.h>
#include <66/service.h>

/**
 * @brief Does one On condition token match a decoded frame? The pure predicate.
 *
 * The On vocabulary IS the status.h vocabulary -- the words 66-supervise projects
 * into the frame -- matched exactly (no aliasing), shared with the parser
 * (on_service_predicate_ok) so the two cannot drift. Keyed on the frame kind:
 *   - a SIGNAL frame matches a signal token (`SIGxxx`, name or number) when the
 *     routed `signo` equals it (via oblibs `sig_parse`);
 *   - a TRANSITION frame matches a bare token that is a status STATE word
 *     (down/starting/up/stopping/finishing/restarting/done/failed) equal to `state`,
 *     or a status RESULT word (success/exited/signaled/timeout-start/timeout-stop/
 *     crash-limit/exec-failed) equal to `result`; plus the two argument predicates
 *     `exited:<n>` (EXITED && code==n) and `signaled:<SIG>` (SIGNALED && code==SIG) ;
 *   - a LIFECYCLE frame never matches an On token (it is not in the vocabulary).
 *
 * The `svc:cond` per-source form is NOT handled here: the caller strips the source
 * prefix and passes the bare condition (a source prefix is disambiguated from an
 * argument like `exited:0` by the caller, which knows the rule's `from` set).
 *
 * @param[in] token A bare condition token (no `svc:` prefix).
 * @param[in] f     The decoded frame.
 * @return 1 if the token matches @f, 0 otherwise (including an unknown token).
 */
extern int eventd_token_match(char const *token, event_frame_t const *f) ;

/**
 * @brief Does frame @f, emitted by service @source, fire rule @r on this frame?
 *
 * Iterates @r's `on` tokens, keeping only those that apply to @source: a bare
 * token (or an argument token like `exited:0`) applies to any source; a `svc:cond`
 * token whose `svc` is in @r's `from` applies only when `svc == source`. Applicable
 * tokens are matched with `eventd_token_match` and combined:
 *   - SINGLE / ANY: fires if ANY applicable token matches (OR) ;
 *   - ALL: returns whether every applicable token FOR THIS SOURCE matches. This is
 *     the single-frame part of an OnAll; the cross-source conjunction (auth up AND
 *     cache ready AND db up) is completed by the daemon, which tracks each source's
 *     latest state -- one frame cannot decide it.
 *
 * @param[in] r      The rule.
 * @param[in] source The name of the service that emitted @f.
 * @param[in] f      The decoded frame.
 * @return 1 if the rule fires (per the note above), 0 otherwise.
 */
extern int eventd_rule_match(resolve_service_addon_event_t const *r, char const *source, event_frame_t const *f) ;

/**
 * @brief Load service @name's [Event] rule -- its `.event` resolve addon -- under @base.
 *
 * Thin wrapper over the public `resolve_read`: it adds the `.event` suffix, opens the
 * addon CDB and reads every `event*` key into @out (which owns its own `sa`). A
 * service with no `.event` addon simply carries no rule (return 0, not an error).
 *
 * @param[in]  base The live/system base the resolve lives under.
 * @param[in]  name The service name.
 * @param[out] out  Zeroed addon to fill (RESOLVE_SERVICE_ADDON_EVENT_ZERO). Its `sa`
 *                  is released with `eventd_rule_free`.
 * @return 1 if a rule was loaded, 0 if the service has no rule, -1 on error.
 */
extern int eventd_rule_load(char const *base, char const *name, resolve_service_addon_event_t *out) ;

/**
 * @brief Release a rule loaded by `eventd_rule_load`.
 *
 * @param[in,out] r The rule to release (its `sa` is freed and it is zeroed).
 *                  A NULL @r is a no-op.
 */
extern void eventd_rule_free(resolve_service_addon_event_t *r) ;

#endif
