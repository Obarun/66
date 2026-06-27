/*
 * 66-scandir.c
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
 * Oblibs port of s6-svscan(skalibs/s6-free). The scanner logic is preserved
 * verbatim from s6 -- the service pool, the two inverse indexes, the `what`
 * trigger flags, scan/start/reap/killthem/remove_service, the logger-by-pipe
 * coupling(peer/p) and the temporize/panic model are unchanged. Only the
 * substrate is swapped:
 *
 *   skalibs avltreen by_pid/by_devino -> oblibs fhash_cb(typed callbacks)
 *   skalibs genset services           -> svpool(free-list, 66-scandir/lib)
 *   skalibs bitarray active           -> oblibs bitset32_t(bits.h, inline value)
 *   the giant stack VLA                -> one bounded, tested heap alloc at boot
 *   skalibs iopause                    -> oblibs SSE epoll loop(deadline as timeout)
 *   skalibs selfpipe                   -> oblibs SSE signal watcher(sse_start_signal)
 *   skalibs tain deadlines             -> CLOCK_MONOTONIC timespec deadlines
 *   skalibs cspawn                     -> oblibs spawn_path_full(posix_spawn)
 *   skalibs djbunix/strerr             -> oblibs fd/io/log
 *
 * Minimal-divergence: the control fifo keeps the native s6 single-byte alphabet
 * (.66-scandir/control), the .66-scandir/{finish,crash,SIG*} scripts and the `max`
 * service bound are kept.
 *
 * Signals: sse_start_signal now delivers one callback per distinct pending signal
 * (level-triggered, no coalescing), so a single signal watcher correctly drives
 * both SIGCHLD reaping AND the per-signal .66-scandir/SIG<name> scripts -- exactly
 * what s6-svscan's handle_signals did, with no manual signalfd draining.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/clock.h>
#include <oblibs/types.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/linux.h> // lx_signalfd_end (restore the signal mask before exec)
#include <oblibs/opt.h>
#include <oblibs/spawn.h>
#include <oblibs/process.h>
#include <oblibs/environ.h>
#include <oblibs/fhash_cb.h>

#include <66/config.h> // SS_MAX_SERVICE, SS_MAX_SERVICE_NAME (compile-time)
#include "lib/scandir.h" // svpool + devino + (via bits.h) bitset32_t

/* active/tmpactive and the pool occupancy are oblibs bitset32_t (inline value type,
 * capacity UINT32_BITS_MAX = 1024 bits). 66's service-count cap is the compile-time
 * SS_MAX_SERVICE, so this must hold; if it is ever raised past 1024 the bitsets would
 * silently overflow -- catch it here rather than at runtime. */
#if SS_MAX_SERVICE > UINT32_BITS_MAX
#error "SS_MAX_SERVICE exceeds bitset32_t capacity (1024); use a wider bitset for active/occupancy"
#endif

#define CTLDIR ".66-scandir"
#define CTL CTLDIR "/control"
#define LCK CTLDIR "/lock"
#define FINISH_PROG CTLDIR "/finish"
#define CRASH_PROG CTLDIR "/crash"
#define SIGNAL_PROG CTLDIR "/SIG"
#define SIGNAL_PROG_LEN (sizeof(SIGNAL_PROG) - 1)
#define SPECIAL_LOGGER_SERVICE "scandir-log"

#define SUPERVISE_BIN SS_BINPREFIX "66-supervise"

typedef struct service_s service ;
struct service_s
{
    devino_t devino ;
    pid_t pid ;
    struct timespec start ;
    int p ;
    uint32_t peer ;
} ;

struct flags_s
{
    uint8_t cont : 1 ;
    uint8_t waitall : 1 ;
} ;

static struct flags_s flags = { .cont = 1, .waitall = 0 } ;

/* the service-count cap and the service-name-length cap are 66 compile-time
 * constants, not runtime options: 66 always launches us with these values. */
static uint32_t const max = SS_MAX_SERVICE ;
static uint32_t const namemax = SS_MAX_SERVICE_NAME ;
static uint32_t special ;

// boot-allocated storage (replaces the s6-svscan stack VLA block)
static service *services = 0 ;
static char *names = 0 ;
static svpool_t pool = SVPOOL_ZERO ;
static fhash_cb_t by_pid ;
static fhash_cb_t by_devino ;
static bitset32_t active = BITSET32_ZERO ;
static bitset32_t tmpactive = BITSET32_ZERO ;

#define SERVICE(i) (services + (i))
#define NAME(i) (names + (size_t)(i) * ((size_t)namemax + 5))

// deadlines live on CLOCK_MONOTONIC (s6-svscan uses a monotonic stopwatch too)
typedef struct deadline_s { struct timespec t ; int inf ; } deadline_t ;
static deadline_t scan_deadline ;
static deadline_t start_deadline ;
static struct timespec scantto ; // relative rescan period
static int scantto_inf = 1 ; // default: no periodic rescan (TAIN_INFINITE_RELATIVE)

// control/signal callback shared state
static unsigned int g_what = 0 ;
static int controlfd = -1 ;
static sse_watcher_t wsig ;
static sse_watcher_t wctl ;

// fhash_cb typed callbacks (aux = the services pool base)
static uint32_t bypid_hash(void const *key, void *aux)
{
    (void)aux ;
    uint32_t h = (uint32_t)(*(pid_t const *)key) ;
    h *= 2654435761u ; // Knuth multiplicative
    return h ;
}

static void const *bypid_keyof(uint32_t v, void *aux)
{
    return &((service *)aux)[v].pid ;
}

static int bypid_eq(void const *a, void const *b, void *aux)
{
    (void)aux ;
    return *(pid_t const *)a == *(pid_t const *)b ;
}

static uint32_t bydevino_hash(void const *key, void *aux)
{
    devino_t const *d = key ;
    (void)aux ;
    uint64_t x = (uint64_t)d->dev * 1099511628211ull ^ (uint64_t)d->ino ;
    x ^= x >> 33 ; x *= 0xff51afd7ed558ccdull ; x ^= x >> 33 ; // fmix64
    return (uint32_t)x ;
}

static void const *bydevino_keyof(uint32_t v, void *aux)
{
    return &((service *)aux)[v].devino ;
}

static int bydevino_eq(void const *a, void const *b, void *aux)
{
    devino_t const *x = a, *y = b ;
    (void)aux ;
    // compare named fields, never raw bytes -> padding/representation independent
    return x->dev == y->dev && x->ino == y->ino ;
}

static inline void dl_epoch(deadline_t *d) { d->t.tv_sec = 0 ; d->t.tv_nsec = 0 ; d->inf = 0 ; }
static inline void dl_inf(deadline_t *d) { d->inf = 1 ; }

static inline void dl_now(deadline_t *d)
{
    clock_now_mono(&d->t) ;
    d->inf = 0 ;
}

static inline void dl_addsec_now(deadline_t *d, int64_t s)
{
    struct timespec now ;
    clock_now_mono(&now) ;
    clock_addsec(&d->t, &now, s) ;
    d->inf = 0 ;
}

static inline int dl_future(deadline_t const *d)
{
    struct timespec now ;
    if (d->inf) return 1 ;
    clock_now_mono(&now) ;
    return clock_cmp(&d->t, &now) > 0 ;
}

// d = min(d, candidate)
static inline void dl_earliest_ts(deadline_t *d, struct timespec const *c)
{
    if (d->inf || clock_cmp(c, &d->t) < 0) {
        d->t = *c ;
        d->inf = 0 ;
    }
}

static inline int ts_future(struct timespec const *t)
{
    struct timespec now ;
    clock_now_mono(&now) ;
    return clock_cmp(t, &now) > 0 ;
}

static int compute_timeout(void)
{
    struct timespec now ;
    int64_t best = -1 ;
    clock_now_mono(&now) ;

    if (!scan_deadline.inf) {
        int64_t ms = (int64_t)(scan_deadline.t.tv_sec - now.tv_sec) * 1000 + ((int64_t)scan_deadline.t.tv_nsec - (int64_t)now.tv_nsec) / 1000000 ;
        best = ms < 0 ? 0 : ms ;
    }

    if (!start_deadline.inf) {
        int64_t ms = (int64_t)(start_deadline.t.tv_sec - now.tv_sec) * 1000 + ((int64_t)start_deadline.t.tv_nsec - (int64_t)now.tv_nsec) / 1000000 ;
        if (ms < 0) ms = 0 ;
        if (best < 0 || ms < best) best = ms ;
    }

    if (best < 0)
        return SSE_TIMEOUT_INFINITE ;

    if (best > INT_MAX)
        best = INT_MAX ;

    return (int)best ;
}

static void set_scan_timeout(unsigned int n)
{
    struct timespec a, now ;
    clock_now_mono(&now) ;
    clock_addsec(&a, &now, n) ;
    dl_earliest_ts(&scan_deadline, &a) ;
}

static void panicnosp(char const *errmsg)
{
    char const *eargv[2] = { CRASH_PROG, 0 } ;
    log_warnusys(errmsg) ;
    log_warn("executing into ", eargv[0]) ;
    execv(eargv[0], (char *const *)eargv) ;
    log_dieusys(LOG_EXIT_SYS, "exec ", eargv[0]) ;
}

static void panic(char const *errmsg)
{
    int e = errno ;
    lx_signalfd_end() ;
    errno = e ;
    panicnosp(errmsg) ;
}

static int close_pipes_iter(uint32_t i, void *aux)
{
    service *sv = SERVICE(i) ;
    (void)aux ;
    if (sv->p >= 0)
        close(sv->p) ;
    return 1 ;
}

static inline void close_pipes(void)
{
    svpool_iter(&pool, &close_pipes_iter, 0) ;
    if (special < max) {

        close_fd(1) ;

        if (io_open("/dev/null", O_WRONLY) < 0)
            log_warnusys("open /dev/null") ;

        if (copy_fd(2, 1) == -1)
            log_warnusys("redirect stderr to /dev/null") ;
    }
}

static inline void waitthem(void)
{
    while (fhash_cb_count(&by_pid)) {

        int wstat ;
        pid_t pid = process_wait(-1, &wstat) ;   /* oblibs: blocking waitpid + EINTR retry */
        if (pid < 0)
        {
            log_warnusys("wait for all 66-supervise processes") ;
            break ;
        }
        fhash_cb_delete(&by_pid, &pid) ;
    }
}

/**
 * what:
 *  1 -> kill all services
 *  2 -> kill inactive services
 *  4 -> send a SIGTERM to loggers instead of SIGHUP
 *  8 -> reap
 * 16 -> delay killing until the next scan
 **/

static inline int is_logger(uint32_t i)
{
    return !!strchr(NAME(i), '/') ;
}

static char const *scandir_signame(int sig)
{
    switch (sig) {
        case SIGHUP : return "HUP" ;
        case SIGINT : return "INT" ;
        case SIGQUIT : return "QUIT" ;
        case SIGTERM : return "TERM" ;
        case SIGUSR1 : return "USR1" ;
        case SIGUSR2 : return "USR2" ;
#ifdef SIGPWR
        case SIGPWR : return "PWR" ;
#endif
#ifdef SIGWINCH
        case SIGWINCH : return "WINCH" ;
#endif
        default : return 0 ;
    }
}

static inline void chld(unsigned int *what)
{
    *what |= 8 ;
}

static inline void alrm(unsigned int *what)
{
    *what |= 16 ;
    dl_now(&scan_deadline) ;
}

static inline void abrt(void)
{
    flags.cont = 0 ;
    flags.waitall = 0 ;
}

static void hup(unsigned int *what)
{
    *what |= 18 ;
    dl_now(&scan_deadline) ;
}

static void term(unsigned int *what)
{
    flags.cont = 0 ;
    flags.waitall = 1 ;
    *what |= 3 ;
}

static void quit(unsigned int *what)
{
    flags.cont = 0 ;
    flags.waitall = 1 ;
    *what |= 7 ;
}

/* One signal per call: the SSE signal watcher delivers a single siginfo per
 * dispatch (level-triggered, so further pending signals re-fire next cycles --
 * no coalescing, no loss). We just act on the one signal handed to us. */
static void handle_one_signal(int sig, unsigned int *what)
{
    switch (sig) {
        case SIGCHLD : chld(what) ; break ;
        case SIGALRM : alrm(what) ; break ;
        case SIGABRT : abrt() ; break ;
        default :
        {
            int usebuiltin = 1 ;
            char const *name = scandir_signame(sig) ;
            if (name) {
                size_t len = strlen(name) ;
                char fn[SIGNAL_PROG_LEN + len + 1] ;
                char const *const newargv[2] = { fn, 0 } ;

                memcpy(fn, SIGNAL_PROG, SIGNAL_PROG_LEN) ;
                memcpy(fn + SIGNAL_PROG_LEN, name, len + 1) ;

                if (access(newargv[0], X_OK) == 0) {
                    /* avoids a spawn, don't care about the toctou */
                    /* posix_spawn forces an empty child sigmask, so the script does
                     * not inherit our signalfd-blocked set. */
                    if (spawn_path_full(newargv[0], newargv, (char const *const *)environ, 0, 0, 0, 0, 0)) {
                        usebuiltin = 0 ;
                    } else if (errno != ENOENT) {
                        log_warnusys("spawn ", newargv[0]) ;
                    }
                }
            }

            if (usebuiltin) switch (sig) {
                case SIGHUP : hup(what) ; break ;
                case SIGINT :
                case SIGTERM : term(what) ; break ;
                case SIGQUIT : quit(what) ; break ;
            }
        }
    }
}

static void handle_control(int fd, unsigned int *what)
{
    for (;;) {
        char c ;
        ssize_t r = io_read_result(io_read(fd, &c, 1)) ;
        if (r < 0) {

            if (errno == EPIPE)
                break ; // EOF: never happens, we hold the write end

            panic("read control pipe") ;

        } else if (!r)
            break ; // would block

        switch (c) {
            case 'z' : chld(what) ; break ;
            case 'a' : alrm(what) ; break ;
            case 'b' : abrt() ; break ;
            case 'h' : hup(what) ; break ;
            case 'i' :
            case 't' : term(what) ; break ;
            case 'q' : quit(what) ; break ;
            case 'n' : *what |= 2 ; break ;
            case 'N' : *what |= 6 ; break ;
            default :
            {
                char s[2] = { c, 0 } ;
                log_warn("received unknown control command: ", s) ;
            }
        }
    }
}


// Triggered action: killer

static int killthem_iter(uint32_t i, void *aux)
{
    service *sv = SERVICE(i) ;
    unsigned int *what = aux ;
    if ((*what & 1 || !bitset32_isvalid(&active, i)) && sv->pid)
        kill(sv->pid, *what & (2 << (i == special || is_logger(i))) ? SIGTERM : SIGHUP) ;
    return 1 ;
}

static inline void killthem(unsigned int *what)
{
    if (*what & 16 || !(*what & 7))
        return ;

    svpool_iter(&pool, &killthem_iter, what) ;

    *what &= ~7 ;
}


/**
 * sv->p values:
 *  0+ : this end of the pipe
 *  -1 : not a logged service
 *  -2 : inactive and peer dead, do not reactivate
 *  -3 : reactivation wanted, trigger rescan on death
 **/

static void remove_service(service *sv)
{
    if (sv->peer < max) {
        service *peer = SERVICE(sv->peer) ;
        if (peer->p >= 0) {
            close(peer->p) ;
            peer->p = -2 ;
        }
        peer->peer = max ;
    }

    if (sv->p == -3) {

        dl_earliest_ts(&scan_deadline, &sv->start) ;

    } else if (sv->p >= 0) {

        close(sv->p) ;
    }

    fhash_cb_delete(&by_devino, &sv->devino) ;
    svpool_delete(&pool, (uint32_t)(sv - services)) ;
}

static void reap(unsigned int *what)
{
    if (!(*what & 8))
        return ;

    *what &= ~8 ;

    for (;;) {
        uint32_t i ;
        int wstat ;
        pid_t pid ;
        do pid = waitpid(-1, &wstat, WNOHANG) ; while (pid == -1 && errno == EINTR) ;
        if (pid < 0) {

            if (errno != ECHILD)
                panic("waitpid") ;

            else break ;

        } else if (!pid) {

            break ;

        } else if (fhash_cb_search(&by_pid, &pid, &i)) {

            service *sv = SERVICE(i) ;
            fhash_cb_delete(&by_pid, &pid) ;
            sv->pid = 0 ;
            if (bitset32_isvalid(&active, i)) {
                dl_earliest_ts(&start_deadline, &sv->start) ;
            } else remove_service(sv) ;
        }
    }
}

static int check(char const *name, uint32_t prod, bitset32_t *act)
{
    struct stat st ;
    devino_t di ;
    uint32_t i ;
    service *sv ;
    if (stat(name, &st) == -1) {

        if (prod < max && errno == ENOENT) {
            if (SERVICE(prod)->peer < max)
                log_warn("logger for service ", NAME(prod), " has been moved") ;
            return (int)max ;
        }
        log_warnusys("stat ", name) ;
        return -4 ;
    }

    if (!S_ISDIR(st.st_mode))
        return (int)max ;

    di.dev = st.st_dev ;
    di.ino = st.st_ino ;

    if (fhash_cb_search(&by_devino, &di, &i)) {
        sv = SERVICE(i) ;
        if (sv->peer < max) {
            if (prod < max && prod != sv->peer) {
                log_warn("old service ", name, " still exists, waiting") ;
                return -10 ;
            }

            if (sv->p == -1) {
                sv->p = -2 ;
                return (int)i ;
            }
        }

    } else {

        i = svpool_new(&pool) ;
        if (i >= max) {
            log_warn("start supervisor for ", name, ": too many services") ;
            return -60 ;
        }

        sv = SERVICE(i) ;
        sv->devino = di ;
        sv->pid = 0 ;

        clock_now_mono(&sv->start) ;
        dl_now(&start_deadline) ; // XXX: may cause a superfluous start if logger fails, oh well

        if (prod >= max) {

            sv->peer = max ;
            sv->p = -1 ;
            if (special >= max && !strcmp(name, SPECIAL_LOGGER_SERVICE))
                special = i ;

        } else {
            int p[2] ;
            if (pipe2(p, O_CLOEXEC) == -1) {
                log_warnusys("create pipe for ", name) ;
                svpool_delete(&pool, i) ;
                return -3 ;
            }
            sv->peer = prod ;
            sv->p = p[0] ;
            SERVICE(prod)->peer = i ;
            SERVICE(prod)->p = p[1] ;
        }
        // devino written above (stable field) -> safe to index by it now
        fhash_cb_insert(&by_devino, &sv->devino, i) ;
    }
    strcpy(NAME(i), name) ;
    bitset32_set(act, i) ;
    return (int)i ;
}

static int remove_deadinactive_iter(uint32_t i, void *aux)
{
    service *sv = SERVICE(i) ;
    (void)aux ;
    if (!bitset32_isvalid(&active, i) && !sv->pid)
        remove_service(sv) ;
    return 1 ;
}

static inline void initial_cleanup(void)
{
    DIR *dir = opendir(".") ;
    if (!dir) log_dieusys(LOG_EXIT_SYS, "opendir .") ;
    for (;;) {
        struct dirent *d ;
        errno = 0 ;
        d = readdir(dir) ;
        if (!d) break ;
        if (d->d_name[0] == '.') continue ;
        if (access(d->d_name, X_OK) == -1 && errno == ENOENT) unlink(d->d_name) ;
    }
    if (errno) log_dieusys(LOG_EXIT_SYS, "readdir .") ;
    closedir(dir) ;
}

static void scan(unsigned int *what)
{
    DIR *dir = opendir(".") ;
    tmpactive = bitset32_init(max) ; // fresh zeroed bitset for this scan pass

    if (scantto_inf) {
        dl_inf(&scan_deadline) ;
    } else {
        struct timespec now ;
        clock_now_mono(&now) ;
        clock_add(&scan_deadline.t, &now, &scantto) ;
        scan_deadline.inf = 0 ;
    }

    if (!dir) {
        log_warnusys("opendir .") ;
        set_scan_timeout(5) ;
        return ;
    }

    for (;;) {
        int i ;
        size_t len ;
        struct dirent *d ;
        errno = 0 ;
        d = readdir(dir) ;
        if (!d) break ;
        if (d->d_name[0] == '.') continue ;
        len = strlen(d->d_name) ;
        if (len > namemax) {
            log_warn("name too long - not spawning service: ", d->d_name) ;
            continue ;
        }

        i = check(d->d_name, max, &tmpactive) ;
        if (i < 0) {
            closedir(dir) ;
            set_scan_timeout((unsigned int)(-i)) ;
            return ;
        }

        if ((uint32_t)i < max) {
            char logname[len + 5] ;
            memcpy(logname, d->d_name, len) ;
            memcpy(logname + len, "/log", 5) ;
            if (check(logname, (uint32_t)i, &tmpactive) < 0) {
                svpool_delete(&pool, (uint32_t)i) ;
                closedir(dir) ;
                /* NB: upstream s6 passes -i here (the producer index, >= 0), not the
                 * inner check()'s negative error code -- ported verbatim (suspected
                 * upstream quirk; minimal-divergence, not silently "fixed"). */
                set_scan_timeout((unsigned int)(-i)) ;
                return ;
            }
        }
    }
    closedir(dir) ;
    if (errno) {
        log_warnusys("readdir .") ;
        set_scan_timeout(5) ;
        return ;
    }
    active = tmpactive ; // value-type copy (was memcpy)
    svpool_iter(&pool, &remove_deadinactive_iter, 0) ;
    *what &= ~16 ;
}

static int start_iter(uint32_t i, void *aux)
{
    service *sv = SERVICE(i) ;
    char const *const cargv[3] = { "66-supervise", NAME(i), 0 } ;
    spawn_fa_t fa[1] ;
    size_t j = 0 ;
    (void)aux ;

    if (!bitset32_isvalid(&active, i) || sv->pid || ts_future(&sv->start))
        return 1 ;

    if (sv->peer < max) {
        fa[j] = (spawn_fa_t){ .type = SPAWN_FA_MOVE, .from = sv->p, .to = !is_logger(i) } ;
        j++ ;
    }

    sv->pid = spawn_path_full(SUPERVISE_BIN, cargv, (char const *const *)environ, 0, 0, SPAWN_FLAG_SETSID, fa, j) ;
    if (!sv->pid) {
        log_warnusys("spawn 66-supervise for ", NAME(i)) ;
        dl_addsec_now(&start_deadline, 10) ;
        return 0 ;
    }

    fhash_cb_insert(&by_pid, &sv->pid, i) ;
    struct timespec now ;
    clock_now_mono(&now) ;
    clock_addsec(&sv->start, &now, 1) ;   // anti-crash-loop: next start >= 1s

    return 1 ;
}

static inline void start(void)
{
    dl_inf(&start_deadline) ;
    svpool_iter(&pool, &start_iter, 0) ;
}

static void alloc_storage(void)
{
    uint32_t nslots = fhash_cb_sizefor(max) ;
    uint32_t *freelist ;
    fhash_cb_slot_t *bypid_slots, *bydevino_slots ;

    if (!nslots) log_die(LOG_EXIT_SYS, "service count too large") ;

    services = malloc(sizeof(service) * (size_t)max) ;
    freelist = malloc(sizeof(uint32_t) * (size_t)max) ;
    names = malloc((size_t)max * ((size_t)namemax + 5)) ;
    bypid_slots = malloc(sizeof(fhash_cb_slot_t) * (size_t)nslots) ;
    bydevino_slots = malloc(sizeof(fhash_cb_slot_t) * (size_t)nslots) ;

    if (!services || !freelist || !names || !bypid_slots || !bydevino_slots)
        log_dieusys(LOG_EXIT_SYS, "allocate service storage") ;

    /* active/tmpactive are inline bitset32_t value types (no allocation); the pool's
     * occupancy bitset is inline too. svpool_init/bitset32_init cap n at 1024 (see
     * the SS_MAX_SERVICE compile-time guard above). */
    if (!svpool_init(&pool, freelist, max))
        log_dieusys(LOG_EXIT_SYS, "init service pool") ;
    active = bitset32_init(max) ;
    tmpactive = bitset32_init(max) ;
    if (!fhash_cb_init(&by_pid, bypid_slots, nslots, &bypid_hash, &bypid_keyof, &bypid_eq, services))
        log_dieusys(LOG_EXIT_SYS, "init by_pid index") ;
    if (!fhash_cb_init(&by_devino, bydevino_slots, nslots, &bydevino_hash, &bydevino_keyof, &bydevino_eq, services))
        log_dieusys(LOG_EXIT_SYS, "init by_devino index") ;
}

static inline int control_init(void)
{
    mode_t m = umask(0) ;
    int fdctl, fdlck, r ;
    if (mkdir(CTLDIR, 0700) < 0) {
        struct stat st ;
        if (errno != EEXIST)
            log_dieusys(LOG_EXIT_SYS, "mkdir " CTLDIR) ;
        if (stat(CTLDIR, &st) < 0)
            log_dieusys(LOG_EXIT_SYS, "stat " CTLDIR) ;
        if (!S_ISDIR(st.st_mode))
            log_die(LOG_EXIT_USER, CTLDIR " exists and is not a directory") ;
    }

    fdlck = io_open_mode(LCK, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0600) ;
    if (fdlck < 0) log_dieusys(LOG_EXIT_SYS, "open " LCK) ;
    r = lock_fd(fdlck, 1, 1) ;
    if (r < 0) log_dieusys(LOG_EXIT_SYS, "lock " LCK) ;
    if (!r) log_die(LOG_EXIT_USER, "another instance of 66-scandir is already running on the same directory") ;

    // fdlck leaks but it's coe

    if (mkfifo(CTL, 0600) < 0) {
        struct stat st ;
        if (errno != EEXIST)
            log_dieusys(LOG_EXIT_SYS, "mkfifo " CTL) ;
        if (stat(CTL, &st) < 0)
            log_dieusys(LOG_EXIT_SYS, "stat " CTL) ;
        if (!S_ISFIFO(st.st_mode))
            log_die(LOG_EXIT_USER, CTL " is not a FIFO") ;
    }

    fdctl = io_open(CTL, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (fdctl < 0)
        log_dieusys(LOG_EXIT_SYS, "open " CTL " for reading") ;

    r = io_open(CTL, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "open " CTL " for writing") ;

    // r leaks but it's coe

    umask(m) ;

    return fdctl ;
}

static void control_cb(sse_watcher_t *w, void *data, int revents)
{
    (void)w ; (void)data ;
    if (revents & (SSE_ERROR | SSE_HUP))
        return ; // held write end: shouldn't fire

    handle_control(controlfd, &g_what) ;
}

static void signal_cb(sse_watcher_t *w, void *data, int revents)
{
    (void)data ;
    if (revents & (SSE_ERROR | SSE_HUP)) panic("signal watcher") ;
    handle_one_signal((int)((sse_signal_t *)w->sdata)->si.ssi_signo, &g_what) ;
}

/* 66-specialised option set: 66 only ever passes -t (rescan period, from the
 * RESCAN boot config) and -d (readiness fd). The service-count and name-length
 * caps are 66 compile-time constants (SS_MAX_SERVICE / SS_MAX_SERVICE_NAME), so
 * s6-svscan's -C/-L/-X are dropped -- this is the 66 scanner, not a general tool. */
static opt_t const opts[] =
{
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
    { .id = 't', .shortname = 't', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "rescan timeout in milliseconds (0 = no periodic rescan)" },
    { .id = 'd', .shortname = 'd', .longname = "notify", .arg = OPT_REQUIRED, .argname = "number", .help = "notify readiness on file descriptor fd (>= 3)" },
} ;

static opt_cmd_t const cmd =
{
    .name = "66-scandir",
    .operands = "scandir",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

int main(int argc, char const *const *argv)
{
    opt_scan_t st = OPT_SCAN_ZERO ;
    unsigned int notif = 0 ;
    char const *scandir ;
    sse_epoll_t ep = SSE_EPOLL_ZERO ;

    PROG = "66-scandir" ;

    for (;;) {

        int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
        if (o == OPT_END)
            break ;

        switch (o) {
            case OPT_ID_HELP :
                return opt_emit_help(cmd.name, &cmd) ;
            case 't' :
            {
                uint32_t t ;
                if (!u32_scan_strict(st.arg, &t))
                    return opt_emit_usage(cmd.name, &cmd) ;
                if (t) { clock_from_ms(&scantto, t) ; scantto_inf = 0 ; }
                break ;
            }
            case 'd' :
            {
                if (!u32_scan_strict(st.arg, &notif))
                    return opt_emit_usage(cmd.name, &cmd) ;
                if (notif < 3)
                    log_die(LOG_EXIT_USER, "notification fd must be 3 or more") ;
                if (fcntl(notif, F_GETFD) == -1)
                    log_dieusys(LOG_EXIT_USER, "invalid notification fd") ;
                break ;
            }
            default :
                return opt_emit_error(cmd.name, &cmd, o, &st) ;
        }
    }

    argc -= st.ind ; argv += st.ind ;
    if (!argc)
        return opt_emit_usage(cmd.name, &cmd) ;

    scandir = argv[0] ;

    special = max ;
    if (!ensure_stdfds())
        log_die(LOG_EXIT_SYS, "sanitize standard fds") ;

    if (chdir(scandir) == -1)
        log_dieusys(LOG_EXIT_SYS, "chdir to ", scandir) ;

    alloc_storage() ;
    controlfd = control_init() ;

    if (!sse_new(&ep, 8))
        log_dieusys(LOG_EXIT_SYS, "create event loop") ;

    /* One signalfd watcher for every trapped signal: SSE now delivers one
     * callback per distinct signal (no coalescing), so SIGCHLD reaping and the
     * per-signal SIG<name> scripts are both correct. */
    if (!sse_start_signal(&ep, &wsig, &signal_cb, NULL, 1))
        log_dieusys(LOG_EXIT_SYS, "init signal watcher") ;

    if (!sse_ignore_signal(&wsig, SIGPIPE)
     || !sse_attach_signal(&wsig, SIGCHLD)
     || !sse_attach_signal(&wsig, SIGALRM)
     || !sse_attach_signal(&wsig, SIGABRT)
     || !sse_attach_signal(&wsig, SIGHUP)
     || !sse_attach_signal(&wsig, SIGINT)
     || !sse_attach_signal(&wsig, SIGTERM)
     || !sse_attach_signal(&wsig, SIGQUIT)
     || !sse_attach_signal(&wsig, SIGUSR1)
     || !sse_attach_signal(&wsig, SIGUSR2)
#ifdef SIGPWR
     || !sse_attach_signal(&wsig, SIGPWR)
#endif
#ifdef SIGWINCH
     || !sse_attach_signal(&wsig, SIGWINCH)
#endif
       )
        log_dieusys(LOG_EXIT_SYS, "trap signals") ;

    if (!sse_start_io(&ep, &wctl, &control_cb, NULL, controlfd, SSE_READ, 0))
        log_dieusys(LOG_EXIT_SYS, "watch control fifo") ;

    initial_cleanup() ;
    if (notif) {
        if (write(notif, "\n", 1) < 0) log_warnusys("notify readiness") ;
        close(notif) ;
    }

    dl_epoch(&scan_deadline) ;     /* scan immediately at startup */
    dl_inf(&start_deadline) ;

    /* From now on, we must not die.
       Temporize on recoverable errors, and panic on serious ones. */

    ep.running = true ;
    while (flags.cont) {

        int n ;
        do {
            sse_sanitize(&ep) ;
        } while (ep.rerun_file) ;

        if (!sse_prepare(&ep))
            log_warnusys("prepare watchers") ;

        n = sse_wait(&ep, compute_timeout()) ;
        if (n < 0) {

            if (errno == EINTR)
                continue ;
            panic("sse wait") ;

        } else if (!n) {

            if (!dl_future(&scan_deadline))
                scan(&g_what) ;

            if (!dl_future(&start_deadline))
                start() ;

        } else sse_dispatch(&ep) ;

        killthem(&g_what) ;
        reap(&g_what) ;
    }

    // Finish phase

    close_pipes() ;
    if (flags.waitall)
        waitthem() ;

    sse_free_signal(&wsig) ; // unblock signals (lx_signalfd_end) before exec'ing finish

    {
        char const *eargv[2] = { FINISH_PROG, 0 } ;
        execv(eargv[0], (char *const *)eargv) ;
        if (errno != ENOENT)
            panicnosp("exec finish script " FINISH_PROG) ;
    }

    _exit(0) ;
}
