/* predicate.c — tests for the event rule engine (eventd_rule.c).
 *
 * Asserts the On vocabulary (the status.h state/result words) token by token against
 * decoded frames built by hand, the argument predicates (exited:n, signaled:SIG), the
 * signal-frame path, per-source filtering and the SINGLE/ANY/ALL combine. Then the
 * on-disk side: eventd_rule_load's 1/0/-1 (round-trip a real .event addon, no addon,
 * corrupt addon) and eventd_rule_free by effect. ASan+UBSan+LSan.
 */
#include "ctest.h"

#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>

#include <string.h>

#include "eventd.h"
#include <66/event.h>
#include <66/status.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>

/* ------------------------------------------------------------------ */
/* frame builders                                                      */
/* ------------------------------------------------------------------ */

static event_frame_t tframe(uint8_t state, uint8_t result, uint32_t code)
{
    event_frame_t f = { 0 } ;
    f.kind = EVENT_KIND_TRANSITION ;
    f.state = state ;
    f.result = result ;
    f.code = code ;
    return f ;
}

static event_frame_t sframe(uint8_t signo)
{
    event_frame_t f = { 0 } ;
    f.kind = EVENT_KIND_SIGNAL ;
    f.signo = signo ;
    return f ;
}

#define M(tok, frame) ({ event_frame_t m_f_ = (frame) ; eventd_token_match((tok), &m_f_) ; })

/* ------------------------------------------------------------------ */
/* eventd_token_match — state vocabulary                                */
/* ------------------------------------------------------------------ */

static void test_states(void)
{
    // a bare state token matches its status.h state EXACTLY (no aliasing)
    T_ASSERT(M("up", tframe(STATUS_STATE_UP, 0, 0)), "up matches UP") ;
    T_ASSERT(!M("up", tframe(STATUS_STATE_STARTING, 0, 0)), "up != STARTING (exact)") ;
    T_ASSERT(!M("up", tframe(STATUS_STATE_DOWN, 0, 0)), "up != DOWN") ;
    T_ASSERT(M("starting", tframe(STATUS_STATE_STARTING, 0, 0)), "starting") ;
    T_ASSERT(M("stopping", tframe(STATUS_STATE_STOPPING, 0, 0)), "stopping") ;
    T_ASSERT(M("finishing", tframe(STATUS_STATE_FINISHING, 0, 0)), "finishing") ;
    T_ASSERT(M("restarting", tframe(STATUS_STATE_RESTARTING, 0, 0)), "restarting") ;
    T_ASSERT(M("done", tframe(STATUS_STATE_DONE, 0, 0)), "done") ;
    T_ASSERT(M("failed", tframe(STATUS_STATE_FAILED, 0, 0)), "failed") ;
    T_ASSERT(M("down", tframe(STATUS_STATE_DOWN, 0, 0)), "down matches DOWN") ;
    T_ASSERT(!M("down", tframe(STATUS_STATE_FINISHING, 0, 0)), "down != FINISHING (exact)") ;
    T_ASSERT(!M("down", tframe(STATUS_STATE_UP, 0, 0)), "down != UP") ;
}

/* ------------------------------------------------------------------ */
/* eventd_token_match — result vocabulary                               */
/* ------------------------------------------------------------------ */

static void test_results(void)
{
    // a bare result token matches its status.h result EXACTLY
    T_ASSERT(M("success", tframe(STATUS_STATE_DONE, STATUS_RESULT_SUCCESS, 0)), "success") ;
    T_ASSERT(M("exited", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1)), "exited") ;
    T_ASSERT(M("signaled", tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, 9)), "signaled") ;
    T_ASSERT(!M("signaled", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1)), "signaled != EXITED") ;
    T_ASSERT(M("timeout-start", tframe(0, STATUS_RESULT_TIMEOUT_START, 0)), "timeout-start") ;
    T_ASSERT(M("timeout-stop", tframe(0, STATUS_RESULT_TIMEOUT_STOP, 0)), "timeout-stop") ;
    T_ASSERT(!M("timeout-start", tframe(0, STATUS_RESULT_TIMEOUT_STOP, 0)), "timeout-start != TIMEOUT_STOP") ;
    T_ASSERT(M("crash-limit", tframe(STATUS_STATE_FAILED, STATUS_RESULT_CRASH_LIMIT, 0)), "crash-limit") ;
    T_ASSERT(M("exec-failed", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXEC_FAILED, 13)), "exec-failed") ;
}

/* ------------------------------------------------------------------ */
/* eventd_token_match — argument tokens (exited:n, signaled:SIG)         */
/* ------------------------------------------------------------------ */

static void test_exited_n(void)
{
    T_ASSERT(M("exited:0", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0)), "exited:0 matches code 0") ;
    T_ASSERT(!M("exited:0", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1)), "exited:0 != code 1") ;
    T_ASSERT(M("exited:37", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 37)), "exited:37") ;
    T_ASSERT(!M("exited:0", tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, 0)), "exited:0 needs EXITED") ;
    T_ASSERT(!M("exited:x", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0)), "exited:<garbage> no match") ;
}

static void test_signaled_sig(void)
{
    event_frame_t k = tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, SIGKILL) ;
    T_ASSERT(M("signaled:9", k), "signaled:9 (numeric) matches SIGKILL") ;
    T_ASSERT(M("signaled:SIGKILL", k), "signaled:SIGKILL matches") ;
    T_ASSERT(!M("signaled:2", k), "signaled:2 != SIGKILL") ;
    T_ASSERT(!M("signaled:9", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 9)), "signaled:9 needs SIGNALED") ;
}

/* the argument-predicate paths that must NOT match: an empty argument, a bare
 * name that is neither a signal name nor a number, and a colon word that does
 * not take an argument at all (a state or a non-argument result). */
static void test_arg_predicates_invalid(void)
{
    // exited: with an empty argument -> u32_scan("") fails -> no match
    T_ASSERT(!M("exited:", tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0)), "exited: (empty arg) no match") ;
    // signaled:<garbage> -> neither a signal name nor a number -> no match, even code 0
    T_ASSERT(!M("signaled:NOPE", tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, 0)), "signaled:NOPE no match") ;
    // a state word with a colon argument is not an argument predicate -> default -> 0
    T_ASSERT(!M("up:1", tframe(STATUS_STATE_UP, 0, 0)), "up:1 (state takes no arg) no match") ;
    // a result word that is not exited/signaled with a colon -> default -> 0
    T_ASSERT(!M("success:x", tframe(STATUS_STATE_DONE, STATUS_RESULT_SUCCESS, 0)), "success:x (no arg) no match") ;
    // a leading-colon token: empty word -> unknown -> 0
    T_ASSERT(!M(":9", tframe(STATUS_STATE_DOWN, STATUS_RESULT_SIGNALED, 9)), "leading-colon token no match") ;
}

/* ------------------------------------------------------------------ */
/* eventd_token_match — signal frames & kind gating                     */
/* ------------------------------------------------------------------ */

static void test_signal_tokens(void)
{
    T_ASSERT(M("SIGHUP", sframe(SIGHUP)), "SIGHUP matches signo HUP") ;
    T_ASSERT(!M("SIGHUP", sframe(SIGTERM)), "SIGHUP != signo TERM") ;
    T_ASSERT(M("SIGUSR1", sframe(SIGUSR1)), "SIGUSR1") ;
    /* a signal token may also be a bare name, a bare number, or a SIG-prefixed number */
    T_ASSERT(M("HUP", sframe(SIGHUP)), "bare name HUP matches signo HUP") ;
    T_ASSERT(M("9", sframe(SIGKILL)), "bare number 9 matches signo KILL") ;
    T_ASSERT(M("SIG9", sframe(SIGKILL)), "SIG9 matches signo KILL") ;
    T_ASSERT(!M("9", sframe(SIGHUP)), "9 != signo HUP") ;
    T_ASSERT(!M("notasignal", sframe(SIGHUP)), "unparsable signal token no match") ;
    /* a service token on a signal frame, and a signal token on a transition frame */
    T_ASSERT(!M("up", sframe(SIGHUP)), "up does not match a SIGNAL frame") ;
    T_ASSERT(!M("SIGHUP", tframe(STATUS_STATE_UP, 0, 0)), "SIGHUP does not match a TRANSITION frame") ;
    /* an out-of-range frame kind (neither SIGNAL nor TRANSITION) matches nothing */
    event_frame_t bad = { 0 } ;
    bad.kind = 99 ;
    bad.state = STATUS_STATE_UP ;
    T_ASSERT(!M("up", bad), "an unknown frame kind matches no token") ;
}

static void test_lifecycle_and_unknown(void)
{
    event_frame_t life = { 0 } ;
    life.kind = EVENT_KIND_LIFECYCLE ;
    life.phase = EVENT_LIFECYCLE_UP ;
    T_ASSERT(!M("up", life), "no On token matches a LIFECYCLE frame") ;
    T_ASSERT(!M("nonsense", tframe(STATUS_STATE_UP, 0, 0)), "unknown token never matches") ;
    T_ASSERT(!M("", tframe(STATUS_STATE_UP, 0, 0)), "empty token never matches") ;
}

/* ------------------------------------------------------------------ */
/* eventd_rule_match — per-source filtering & combine             */
/* ------------------------------------------------------------------ */

/* append a space-joined list (the resolve list convention) from C strings into the
 * rule's own sa and return its offset, exactly as the parser's parse_compute_list
 * would. The rule IS the public resolve_service_addon_event_t. */
static uint32_t add_list(resolve_wrapper_t_ref w, char const *const *items, uint32_t n)
{
    char buf[128] ;
    char *p = buf ;
    for (uint32_t i = 0 ; i < n ; i++) {
        if (i)
            *p++ = ' ' ;
        size_t l = strlen(items[i]) ;
        memcpy(p, items[i], l) ;
        p += l ;
    }
    *p = 0 ;
    return (uint32_t)resolve_add_string(w, buf) ;
}

static void test_single_any(void)
{
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "auth", "db" } ;
    char const *on[] = { "up", "down" } ;
    r.from = add_list(w, from, 2) ; r.nfrom = 2 ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;

    r.combine = EVENT_COMBINE_ANY ;
    event_frame_t up = tframe(STATUS_STATE_UP, 0, 0) ;
    T_ASSERT(eventd_rule_match(&r, "auth", &up), "ANY(up,down): UP matches up") ;
    event_frame_t stopping = tframe(STATUS_STATE_STOPPING, 0, 0) ;
    T_ASSERT(!eventd_rule_match(&r, "auth", &stopping), "ANY(up,down): STOPPING matches neither") ;

    /* SINGLE with one condition */
    char const *on2[] = { "up" } ;
    r.on = add_list(w, on2, 1) ; r.non = 1 ;
    r.combine = EVENT_COMBINE_ANY ;
    T_ASSERT(eventd_rule_match(&r, "db", &up), "SINGLE up: UP from db matches") ;
    T_ASSERT(!eventd_rule_match(&r, "db", &stopping), "SINGLE up: STOPPING no match") ;

    resolve_free(w) ;
}

static void test_per_source(void)
{
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "auth", "db" } ;
    char const *on[] = { "auth:up", "db:down" } ;
    r.from = add_list(w, from, 2) ; r.nfrom = 2 ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ALL ;

    event_frame_t up = tframe(STATUS_STATE_UP, 0, 0) ;
    event_frame_t down = tframe(STATUS_STATE_DOWN, 0, 0) ;

    /* only the token for the emitting source is applicable */
    T_ASSERT(eventd_rule_match(&r, "auth", &up), "auth:up applies to auth, UP matches") ;
    T_ASSERT(!eventd_rule_match(&r, "auth", &down), "auth:up applies to auth, DOWN no match") ;
    T_ASSERT(eventd_rule_match(&r, "db", &down), "db:down applies to db, DOWN matches") ;
    /* a frame from a source not carrying an applicable token fires nothing */
    T_ASSERT(!eventd_rule_match(&r, "other", &up), "no applicable token for 'other'") ;

    resolve_free(w) ;
}

static void test_per_source_namespaced(void)
{
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "evt@test:auth", "evt@test:db" } ;
    char const *on[] = { "evt@test:auth:up", "evt@test:db:down" } ;
    r.from = add_list(w, from, 2) ; r.nfrom = 2 ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ALL ;

    event_frame_t up = tframe(STATUS_STATE_UP, 0, 0) ;
    event_frame_t down = tframe(STATUS_STATE_DOWN, 0, 0) ;

    T_ASSERT(eventd_rule_match(&r, "evt@test:auth", &up), "evt@test:auth:up applies to evt@test:auth, UP matches") ;
    T_ASSERT(!eventd_rule_match(&r, "evt@test:auth", &down), "evt@test:auth:up applies to evt@test:auth, DOWN no match") ;
    T_ASSERT(eventd_rule_match(&r, "evt@test:db", &down), "evt@test:db:down applies to evt@test:db, DOWN matches") ;
    // the bare member name is not the name the source is registered under
    T_ASSERT(!eventd_rule_match(&r, "auth", &up), "no applicable token for the bare member name") ;

    resolve_free(w) ;
}

static void test_per_source_arg(void)
{
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "web", "evt@test:web" } ;
    char const *on[] = { "web:exited:0", "evt@test:web:signaled:SIGKILL" } ;
    r.from = add_list(w, from, 2) ; r.nfrom = 2 ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ANY ;

    event_frame_t e0 = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0) ;
    T_ASSERT(eventd_rule_match(&r, "web", &e0), "web:exited:0 applies to web, code 0 matches") ;
    event_frame_t e1 = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1) ;
    T_ASSERT(!eventd_rule_match(&r, "web", &e1), "web:exited:0 does not match code 1") ;

    event_frame_t k = tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, SIGKILL) ;
    T_ASSERT(eventd_rule_match(&r, "evt@test:web", &k), "evt@test:web:signaled:SIGKILL applies to the namespaced source") ;
    event_frame_t t = tframe(STATUS_STATE_FINISHING, STATUS_RESULT_SIGNALED, SIGTERM) ;
    T_ASSERT(!eventd_rule_match(&r, "evt@test:web", &t), "evt@test:web:signaled:SIGKILL does not match SIGTERM") ;
    // each token stays scoped to its own source
    T_ASSERT(!eventd_rule_match(&r, "web", &k), "the namespaced token does not apply to web") ;

    resolve_free(w) ;
}

static void test_arg_token_is_not_per_source(void)
{
    /* exited:0 has a colon but 'exited' is not a source name: it is a bare arg token */
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "web" } ;
    char const *on[] = { "exited:0" } ;
    r.from = add_list(w, from, 1) ; r.nfrom = 1 ;
    r.on = add_list(w, on, 1) ; r.non = 1 ;
    r.combine = EVENT_COMBINE_ANY ;

    event_frame_t e0 = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0) ;
    T_ASSERT(eventd_rule_match(&r, "web", &e0), "exited:0 applies to the emitting source") ;
    event_frame_t e1 = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1) ;
    T_ASSERT(!eventd_rule_match(&r, "web", &e1), "exited:0 does not match code 1") ;

    resolve_free(w) ;
}

/* combine ALL over two bare tokens applicable to the same frame: every applicable
 * token must match for the rule to fire on this single frame. */
static void test_combine_all_multi(void)
{
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *on[] = { "down", "exited:0" } ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ALL ;

    event_frame_t both = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 0) ;
    T_ASSERT(eventd_rule_match(&r, "web", &both), "ALL(down,exited:0): both match -> fires") ;
    // flip one token's precondition: exited:0 no longer matches (code 1)
    event_frame_t one = tframe(STATUS_STATE_DOWN, STATUS_RESULT_EXITED, 1) ;
    T_ASSERT(!eventd_rule_match(&r, "web", &one), "ALL(down,exited:0): exited:0 fails -> no fire") ;
    // the other token fails: state is UP, down no longer matches
    event_frame_t other = tframe(STATUS_STATE_UP, STATUS_RESULT_EXITED, 0) ;
    T_ASSERT(!eventd_rule_match(&r, "web", &other), "ALL(down,exited:0): down fails -> no fire") ;

    // same tokens under ANY: one match is enough
    r.combine = EVENT_COMBINE_ANY ;
    T_ASSERT(eventd_rule_match(&r, "web", &other), "ANY(down,exited:0): exited:0 alone fires") ;

    resolve_free(w) ;
}

static void test_empty_on(void)
{
    // non == 0 -> eventd_rule_match returns before touching the (empty) sa
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    event_frame_t up = tframe(STATUS_STATE_UP, 0, 0) ;
    T_ASSERT(!eventd_rule_match(&r, "x", &up), "a rule with no On fires nothing") ;
}

/* ------------------------------------------------------------------ */
/* eventd_rule_free — by effect                                         */
/* ------------------------------------------------------------------ */

static void test_free(void)
{
    // build a rule that owns a heap sa, then free it: the sa is released (LSan
    // would flag a leak if strbuf_free were skipped) and every field is zeroed.
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *on[] = { "up", "down" } ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ALL ;
    free(w) ; // release only the wrapper; the sa is owned by r and freed below

    T_ASSERT(r.sa.s != 0, "precondition: the rule owns a heap sa") ;
    eventd_rule_free(&r) ;
    T_ASSERT(r.sa.s == 0, "free releases and clears sa.s") ;
    T_ASSERT_EQ(0, r.sa.len, "free clears sa.len") ;
    T_ASSERT_EQ(0, r.non, "free zeroes non") ;
    T_ASSERT_EQ(0, r.on, "free zeroes on") ;
    T_ASSERT_EQ(0, r.combine, "free zeroes combine") ;

    eventd_rule_free(0) ; // NULL is a no-op (must not crash)
}

/* ------------------------------------------------------------------ */
/* eventd_rule_load — 1 / 0 / -1, by effect                             */
/* ------------------------------------------------------------------ */

/* mkdir -p the nested resolve dir tree resolve_read walks for @name under @base
 * (base has a trailing slash): <base>system/.resolve/service/<name>/.resolve/ */
static void make_service_dirs(char const *base, char const *name)
{
    char p[SS_MAX_PATH_LEN] ;
    size_t n = (size_t)snprintf(p, sizeof p, "%ssystem", base) ; mkdir(p, 0755) ;
    n = (size_t)snprintf(p, sizeof p, "%ssystem/.resolve", base) ; mkdir(p, 0755) ;
    n = (size_t)snprintf(p, sizeof p, "%ssystem/.resolve/service", base) ; mkdir(p, 0755) ;
    n = (size_t)snprintf(p, sizeof p, "%ssystem/.resolve/service/%s", base, name) ; mkdir(p, 0755) ;
    n = (size_t)snprintf(p, sizeof p, "%ssystem/.resolve/service/%s/.resolve", base, name) ; mkdir(p, 0755) ;
    (void)n ;
}

/* write a minimal core service resolve for @name, so core_set_has can flip
 * has_event when the event addon is written. */
static void write_core(char const *base, char const *name)
{
    resolve_service_t core = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE, &core) ;
    resolve_init(w) ;
    core.name = (uint32_t)resolve_add_string(w, name) ;
    T_ASSERT(resolve_write(w, base, name) == 1, "core service resolve written") ;
    resolve_free(w) ;
}

static void test_load_roundtrip(void)
{
    char base[] = "/tmp/ev_load_XXXXXX" ;
    T_ASSERT(t_tmpdir(base) != 0, "tmpdir created") ;
    char bs[64] ;
    snprintf(bs, sizeof bs, "%s/", base) ;

    make_service_dirs(bs, "svc") ;
    write_core(bs, "svc") ;

    // write the .event addon through the real resolve API (as the parser would)
    resolve_service_addon_event_t r = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_EVENT, &r) ;
    resolve_init(w) ;
    char const *from[] = { "auth", "db" } ;
    char const *on[] = { "auth:up", "db:down" } ;
    r.from = add_list(w, from, 2) ; r.nfrom = 2 ;
    r.on = add_list(w, on, 2) ; r.non = 2 ;
    r.combine = EVENT_COMBINE_ALL ;
    r.type = EVENT_SOURCE_SERVICE ;
    r.docmd = EVENT_DO_RESTART ;
    T_ASSERT(resolve_write(w, bs, "svc") == 1, "event addon written") ;
    resolve_free(w) ;

    // load it back and assert the fields round-trip EXACTLY (by effect)
    resolve_service_addon_event_t out = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    int ret = eventd_rule_load(bs, "svc", &out) ;
    T_ASSERT_EQ(1, ret, "load returns 1 for a service carrying a .event addon") ;
    T_ASSERT_EQ(2, out.non, "loaded non") ;
    T_ASSERT_EQ(2, out.nfrom, "loaded nfrom") ;
    T_ASSERT_EQ(EVENT_COMBINE_ALL, out.combine, "loaded combine") ;
    T_ASSERT_EQ(EVENT_SOURCE_SERVICE, out.type, "loaded type") ;
    T_ASSERT_EQ(EVENT_DO_RESTART, out.docmd, "loaded docmd") ;
    T_ASSERT(out.sa.s != 0, "loaded sa is populated") ;
    T_ASSERT(!strcmp(out.sa.s + out.on, "auth:up db:down"), "loaded on list bytes") ;
    T_ASSERT(!strcmp(out.sa.s + out.from, "auth db"), "loaded from list bytes") ;
    eventd_rule_free(&out) ;
}

static void test_load_no_addon(void)
{
    char base[] = "/tmp/ev_noadd_XXXXXX" ;
    T_ASSERT(t_tmpdir(base) != 0, "tmpdir created") ;
    char bs[64] ;
    snprintf(bs, sizeof bs, "%s/", base) ;

    // pre-dirty out: load must both return 0 AND zero it (the *out = ZERO on entry)
    resolve_service_addon_event_t out = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    out.non = 7 ; out.combine = 3 ;
    int ret = eventd_rule_load(bs, "ghost", &out) ;
    T_ASSERT_EQ(0, ret, "load returns 0 when the service has no .event addon") ;
    T_ASSERT_EQ(0, out.non, "load zeroed the output on the no-addon path") ;
    T_ASSERT_EQ(0, out.combine, "load zeroed combine on the no-addon path") ;
    T_ASSERT(out.sa.s == 0, "load left no sa on the no-addon path") ;
    eventd_rule_free(&out) ;
}

static void test_load_corrupt(void)
{
    char base[] = "/tmp/ev_corrupt_XXXXXX" ;
    T_ASSERT(t_tmpdir(base) != 0, "tmpdir created") ;
    char bs[64] ;
    snprintf(bs, sizeof bs, "%s/", base) ;

    make_service_dirs(bs, "bad") ;
    // an empty .event file is not a valid CDB: resolve_open_cdb -> -1 (error path)
    char f[SS_MAX_PATH_LEN] ;
    snprintf(f, sizeof f, "%ssystem/.resolve/service/bad/.resolve/bad.event", bs) ;
    int fd = open(f, O_CREAT | O_WRONLY, 0644) ;
    T_ASSERT(fd >= 0, "empty .event file created") ;
    close(fd) ;

    resolve_service_addon_event_t out = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    int ret = eventd_rule_load(bs, "bad", &out) ;
    T_ASSERT_EQ(-1, ret, "load returns -1 on a corrupt (unreadable-as-cdb) addon") ;
    eventd_rule_free(&out) ;
}

T_SUITE("event predicate")
{
    T_RUN(test_states) ;
    T_RUN(test_results) ;
    T_RUN(test_exited_n) ;
    T_RUN(test_signaled_sig) ;
    T_RUN(test_arg_predicates_invalid) ;
    T_RUN(test_signal_tokens) ;
    T_RUN(test_lifecycle_and_unknown) ;
    T_RUN(test_single_any) ;
    T_RUN(test_per_source) ;
    T_RUN(test_per_source_namespaced) ;
    T_RUN(test_per_source_arg) ;
    T_RUN(test_arg_token_is_not_per_source) ;
    T_RUN(test_combine_all_multi) ;
    T_RUN(test_empty_on) ;
    T_RUN(test_free) ;
    T_RUN(test_load_roundtrip) ;
    T_RUN(test_load_no_addon) ;
    T_RUN(test_load_corrupt) ;
}
