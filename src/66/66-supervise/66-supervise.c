/*
 * 66-supervise.c
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
 * Native oblibs process supervisor. It runs ./run for a service and restarts it
 * per the actions[state][trans] table, with the anti-crash-loop (nextstart >= 1s)
 * and the up/down/finish semantics. The I/O substrate is fully oblibs: the status
 * file is the native 66 service_status_t (clock_pack, big-endian, no TAI64N),
 * projected from the in-RAM runtime flags at each announce():
 *
 *   the event loop      -> oblibs SSE epoll loop (deadline as poll timeout)
 *   child reaping        -> per-child pidfd watcher (sse_start_child), which
 *                           removes the SIGCHLD/wait_pid_nohang race
 *   control signals      -> signalfd watcher (sse_start_signal) for TERM/HUP/QUIT/INT
 *   deadlines            -> CLOCK_MONOTONIC timespec deadline
 *   process spawning     -> oblibs spawn_path_full (posix_spawn)
 *   fd/error helpers     -> oblibs fd/io/log
 *   event broadcast      -> 66 event_fifodir_* (lib66/event)
 *   status file          -> 66 service_status_t (lib66/status, clock_pack)
 *
 * The death-tally file is dropped (the native crash budget lives inline in the
 * status record, added later). The control fifo carries a fixed <op><who> pair
 * per command (op from the command alphabet, who a provenance byte, the two
 * spaces disjoint); the event fifodir keeps the single transition bytes (see SPECS).
 *
 * CONFIG COMES FROM THE 66 RESOLVE, NOT FROM PER-SERVICE FILES. Where a classic
 * supervisor re-reads notification-fd / timeout-finish / timeout-kill /
 * max-death-tally / down-signal from files in the service directory, 66 already
 * has every value in the service resolve (the CDB the parser compiles). We are run
 * by the scandir (66-scandir) with CWD = scandir and the service NAME as argv[1]:
 * we chdir into the service dir by name, then read the resolve LOCALLY from that
 * dir (./.resolve) -- never from the global system CDB. This keeps the supervisor
 * dependency-free at boot (no global lookup), and the scandir-internal services
 * (oneshotd, fdholder, scandir-log, 66-shutdownd) carry a hand-built minimal
 * resolve in their own dir without ever appearing in the system graph. Config is
 * pulled from the struct:
 *   notification-fd -> res.notify
 *   timeout-kill    -> res.execute.timeout.start
 *   timeout-finish  -> res.execute.timeout.stop
 *   max-death-tally -> res.maxdeath
 *   down-signal     -> res.execute.downsignal
 * Only the `down` file stays a file: it is runtime state (66 start/stop toggle it
 * via adddown/deldown), seeded from res.execute.down by the writer at parse time.
 * The lock-fd feature is dropped (66 never produced it). flag-newpidns is
 * deferred (posix_spawn cannot CLONE_NEWPID; needs a fork/unshare path).
 *
 * Known divergences (documented):
 *   - supervise/lock uses an inline fcntl POSIX lock (not oblibs lock_fd, which is
 *     flock-based) to keep the POSIX-lock semantics.
 */

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/sse.h>
#include <oblibs/clock.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/files.h>
#include <oblibs/spawn.h>
#include <oblibs/environ.h>

#include <66/event.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/status.h>
#include <66/utils.h>

#define USAGE "66-supervise servicename"

#define SUPERVISE_PATH_MAX 512

typedef enum trans_e trans_t, *trans_t_ref ;
enum trans_e
{
  V_TIMEOUT, V_CHLD, V_TERM, V_HUP, V_QUIT, V_INT,
  V_a, V_b, V_q, V_h, V_k, V_t, V_i, V_1, V_2, V_p, V_c, V_y, V_r, V_l, V_P, V_C, V_K,
  V_o, V_d, V_u, V_D, V_U, V_x, V_O, V_Q
} ;

typedef enum state_e state_t, *state_t_ref ;
enum state_e
{
  DOWN,
  UP,
  FINISH,
  LASTUP,
  LASTFINISH
} ;

struct gflags_s
{
  uint8_t cont : 1 ;
  uint8_t dying : 1 ;
} gflags =
{
  .cont = 1,
  .dying = 0
} ;

typedef void action_t(void) ;
typedef action_t *action_t_ref ;

// deadline lives on CLOCK_MONOTONIC; deadline_infinite is the "no deadline" sentinel.
static struct timespec deadline ;
static int deadline_infinite = 0 ;
static struct timespec nextstart = { 0, 0 } ;
static int nextstart_set = 0 ;

static struct runtime_s
{
  pid_t pid ;
  int wstat ;
  struct timespec stamp ;      // REALTIME: entry into the current state
  struct timespec readystamp ; // REALTIME: transition to ready
  uint8_t flagpaused ;
  uint8_t flagfinishing ;
  uint8_t flagwantup ;
  uint8_t flagready ;
  uint8_t flagfailed ;        // crash budget exhausted: parked, projects FAILED
  uint8_t result ;            // status_result_e: what last happened to the process
  struct timespec window_start ; // MONOTONIC: start of the current crash window
  uint8_t ndeaths ;           // run-then-die count since window_start
  uint8_t exec_backoff ;      // shift for the exec-failed retry delay (1<<shift, capped 60s)
  int exec_errno ;            // errno of the last failed exec, projected on EXEC_FAILED
} status = { .flagwantup = 1 } ; // wants up by default unless a ./down file exists

/** the death we caused with a timeout kill, consumed at the next process death
 * to override SIGNALED: 1 = uptimeout (TIMEOUT_START), 2 = finishtimeout (TIMEOUT_STOP). */
static uint8_t kill_timeout = 0 ;

/** provenance of the current state, latched on the op that actually flips wantup
 * (anti-race): current_who is the who of the control op being dispatched;
 * latched_who keeps the who of the last real transition (up or down) and is the
 * value projected into the record. It defaults to SELF (a service that comes up
 * on its own owns its transition); a command that changes wantup overwrites it,
 * and an intrinsic death resets it to SELF. Distinguishing a boot/event-driven
 * up from a user up requires the origin to be threaded explicitly from the
 * top-level caller (ssexec_boot/eventd) down to svc_launch, which is not done
 * yet, so we stay honest with SELF rather than guessing BOOT.
 * pending_op buffers an op whose who byte has not arrived yet. */
static uint8_t current_who = STATUS_WHO_USER ;
static uint8_t latched_who = STATUS_WHO_SELF ;
static int pending_op = -1 ;
static int finish_wstat ;
static state_t state = DOWN ;
static char const *servicename = 0 ;

static rlim_t maxfd ;

/** supervise/ leaf paths, relative to the CWD (the service dir), built once at
 * startup from the general constants -- the supervisor never hardcodes them. */
static char status_file[SS_SUPERVISEDIR_LEN + 1 + SS_STATUS_LEN + 1] ;
static char control_file[SS_SUPERVISEDIR_LEN + 1 + SS_CONTROL_LEN + 1] ;
static char lock_file[SS_SUPERVISEDIR_LEN + 1 + SS_LOCK_LEN + 1] ;

/** the service resolve, loaded once at startup: the source of all config that was
 * historically read from per-service files (notify, timeouts, maxdeath, downsignal). */
static resolve_service_t res = RESOLVE_SERVICE_ZERO ;

// SSE loop and its watchers
static sse_epoll_t g_epoll = SSE_EPOLL_ZERO ;
static sse_watcher_t wsignal ; // control signals (TERM/HUP/QUIT/INT)
static sse_watcher_t wcontrol ; // control fifo
static sse_watcher_t wnotify ; // readiness pipe from the child
static sse_watcher_t wchild ; // current child, via pidfd
static int controlfd = -1 ;
static int notifyfd = -1 ;
static int notify_active = 0 ;
static int child_active = 0 ;
/** set by callbacks, consumed by the main loop (so a watcher is never freed and
 * reused from inside its own dispatch) */
static int child_died = 0 ;
static int child_wstat = 0 ;
static int notify_drop_req = 0 ;


/** fcntl POSIX locks (oblibs lock_fd is flock-based, a different and incompatible
 * mechanism, so the lock is done inline here). */

static int fd_lock(int fd, int w, int nb)
{
  struct flock fl = { .l_type = w ? F_WRLCK : F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 } ;
  int e = errno ;
  int r ;
  do r = fcntl(fd, nb ? F_SETLK : F_SETLKW, &fl) ;
  while (r == -1 && errno == EINTR) ;
  if (r != -1) return 1 ;
  if (errno == EACCES || errno == EAGAIN || errno == EWOULDBLOCK) return (errno = e, 0) ;
  return -1 ;
}


// deadline helpers

static inline void settimeout(int secs)
{
  struct timespec now ;
  clock_now_mono(&now) ;
  clock_addsec(&deadline, &now, secs) ;
  deadline_infinite = 0 ;
}

static inline void settimeout_infinite(void)
{
  deadline_infinite = 1 ;
}

static inline void settimeout_ms(uint32_t ms)
{
  struct timespec now, d ;
  clock_now_mono(&now) ;
  clock_from_ms(&d, ms) ;
  clock_add(&deadline, &now, &d) ;
  deadline_infinite = 0 ;
}

static int compute_timeout(void)
{
  if (deadline_infinite) return SSE_TIMEOUT_INFINITE ;
  struct timespec now ;
  clock_now_mono(&now) ;
  int64_t ms = (int64_t)(deadline.tv_sec - now.tv_sec) * 1000 + ((int64_t)deadline.tv_nsec - (int64_t)now.tv_nsec) / 1000000 ;
  if (ms < 0) ms = 0 ;
  if (ms > INT_MAX) ms = INT_MAX ;
  return (int)ms ;
}

static void project_status(service_status_t *st)
{
  st->pid = (uint32_t)status.pid ;
  st->stamp = status.stamp ;
  st->readystamp = status.readystamp ;

  st->result = status.result ;
  if (status.result == STATUS_RESULT_SIGNALED) st->code = (uint32_t)WTERMSIG(status.wstat) ;
  else if (status.result == STATUS_RESULT_EXITED) st->code = (uint32_t)WEXITSTATUS(status.wstat) ;
  else if (status.result == STATUS_RESULT_EXEC_FAILED) st->code = (uint32_t)status.exec_errno ;
  else st->code = 0 ;

  st->who = latched_who ;
  st->window_start = status.window_start ;
  st->ndeaths = status.ndeaths ;

  /* flagfailed (crash budget exhausted) is terminal and parked: it outranks
   * every transient FSM state. flagfinishing is the canonical "the ./finish
   * script is running" signal: it is set before uplastup_z announces, while the
   * FSM state is still UP/LASTUP (up_z/lastup_z set FINISH only after). Read both
   * first so a failed/finishing service never projects as DOWN/STOPPING. */
  if (status.flagfailed)
    st->state = STATUS_STATE_FAILED ;
  else if (status.flagfinishing)
    st->state = STATUS_STATE_FINISHING ;
  else switch (state)
  {
    case UP :
    case LASTUP :
      if (!status.flagwantup) st->state = STATUS_STATE_STOPPING ;
      else if (res.notify && !status.flagready) st->state = STATUS_STATE_STARTING ;
      else st->state = STATUS_STATE_UP ;
      break ;
    case DOWN :
    default :
      st->state = (status.flagwantup && nextstart_set) ? STATUS_STATE_RESTARTING : STATUS_STATE_DOWN ;
      break ;
  }
}

static inline void announce(void)
{
  service_status_t st = STATUS_ZERO ;
  project_status(&st) ;
  if (!status_write(&st, status_file))
    log_warnusys("write status file") ;
}

static int read_file(char const *file, char *buf, size_t n)
{
  int fd = io_open(file, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
  if (fd < 0)
  {
    if (errno != ENOENT) log_warnusys("open ", file) ;
    return 0 ;
  }
  ssize_t r = io_readnclose(fd, buf, n) ; // closes fd; r >= 0 includes a short EOF read
  if (r < 0)
  {
    log_warnusys("read ", file) ;
    return 0 ;
  }
  {
    char *nl = memchr(buf, '\n', (size_t)r) ;
    buf[nl ? (size_t)(nl - buf) : (size_t)r] = 0 ;
  }
  return 1 ;
}

static inline int read_downsig(void)
{
  return res.execute.downsignal ? (int)res.execute.downsignal : SIGTERM ;
}

static inline int read_reloadsig(void)
{
  int sig = SIGHUP ;
  char buf[64] ;
  if (read_file("reload-signal", buf, sizeof(buf) - 1) && !sig_parse(buf, &sig))
    log_warn("invalid ", "reload-signal") ;
  return sig ;
}

static void set_down_and_ready(event_t const *ev, unsigned int n)
{
  status.pid = 0 ;
  status.flagfinishing = 0 ;
  status.flagready = 1 ;
  state = DOWN ;
  if (nextstart_set) { deadline = nextstart ; deadline_infinite = 0 ; }
  else settimeout(1) ;
  clock_now(&status.readystamp) ;
  announce() ;
  event_fifodir_emit(SS_EVENTDIR + 1, ev, n) ;
}


// readiness pipe and child watcher teardown (main-loop context only)

static void drop_notifyfd(void)
{
  if (notify_active)
  {
    sse_free_io(&wnotify) ; // closes notifyfd
    notify_active = 0 ;
    notifyfd = -1 ;
  }
  else if (notifyfd >= 0)
  {
    close_fd(notifyfd) ;
    notifyfd = -1 ;
  }
}

// The action array.

static void nop(void){}

static void bail(void)
{
  gflags.cont = 0 ;
}

static void killI(void)
{
  killpg(status.pid, SIGINT) ;
}

static void sigint(void)
{
  killI() ;
  bail() ;
}

static void closethem(void)
{
  close_fd(0) ;
  close_fd(1) ;
  if (io_open("/dev/null", O_RDONLY) != 0)
    log_warnusys("open /dev/null for ", "reading") ;
  else if (io_open("/dev/null", O_WRONLY | O_NONBLOCK) != 1 || io_set_block(1) < 0)
    log_warnusys("open /dev/null for ", "writing") ;
}

static void adddown(void)
{
  if (!file_write_atomic("down", "", 0))
    log_warnusys("create ", "./down file") ;
}

static void deldown(void)
{
  int e = errno ;
  if (unlink("down") == -1 && errno != ENOENT)
    log_warnusys("unlink ", "./down file") ;
  errno = e ;
}

static void killa(void) { kill(status.pid, SIGALRM) ; }
static void killb(void) { kill(status.pid, SIGABRT) ; }
static void killh(void) { kill(status.pid, SIGHUP) ; }
static void killq(void) { kill(status.pid, SIGQUIT) ; }
static void killk(void) { kill(status.pid, SIGKILL) ; }
static void killt(void) { kill(status.pid, SIGTERM) ; }
static void killi(void) { kill(status.pid, SIGINT) ; }
static void kill1(void) { kill(status.pid, SIGUSR1) ; }
static void kill2(void) { kill(status.pid, SIGUSR2) ; }

static void killp(void)
{
  kill(status.pid, SIGSTOP) ;
  status.flagpaused = 1 ;
  announce() ;
}

static void killc(void)
{
  kill(status.pid, SIGCONT) ;
  status.flagpaused = 0 ;
  announce() ;
}

static void killy(void) { kill(status.pid, SIGWINCH) ; }

static void killr(void)
{
  kill(status.pid, read_downsig()) ;
}

static void killl(void)
{
  kill(status.pid, read_reloadsig()) ;
}

static void killP(void)
{
  killpg(status.pid, SIGSTOP) ;
  status.flagpaused = 1 ;
  announce() ;
}

static void killC(void)
{
  killpg(status.pid, SIGCONT) ;
  status.flagpaused = 0 ;
  announce() ;
}

static void killK(void) { killpg(status.pid, SIGKILL) ; }

static void child_cb(sse_watcher_t *w, void *data, int revents)
{
  log_flow() ;
  (void)data ; (void)revents ;
  /** a CHILD callback fires only when the pidfd becomes readable, i.e. the child
   * has exited; sse_dispatch_child has already reaped it and stored the status */
  child_wstat = ((sse_child_t *)w->sdata)->status ;
  child_died = 1 ;
}

static void notify_cb(sse_watcher_t *w, void *data, int revents)
{
  log_flow() ;
  (void)w ; (void)data ;
  /* Drain readable data FIRST, then honour a hangup. A child that writes its
   * readiness newline and closes the fd at once (e.g. 66-log) makes the pipe
   * report POLLIN and POLLHUP together; acting on the hangup before reading
   * would drop the pending newline and the service would never be seen ready. */
  for (;;) {
    char buf[512] ;
    ssize_t r = io_read_result(io_read(notifyfd, buf, 512)) ;
    if (r < 0) { notify_drop_req = 1 ; return ; } // EOF (EPIPE) or error: stop watching
    if (!r) break ; // would block: nothing more to read for now
    if (memchr(buf, '\n', (size_t)r)) {
      struct timespec now ;
      clock_now_mono(&now) ;
      clock_addsec(&nextstart, &now, 1) ;
      nextstart_set = 1 ;
      clock_now(&status.readystamp) ;
      status.flagready = 1 ;
      announce() ;
      event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_READY}, 1) ;
      notify_drop_req = 1 ;
      return ;
    }
  }

  if (revents & (SSE_ERROR | SSE_HUP))
    notify_drop_req = 1 ; // hangup, no newline: stop watching
}

// native crash budget (variant B: fixed re-armed window)

static int within_window(struct timespec const *start, struct timespec const *now, uint32_t ms)
{
  int64_t elapsed = (int64_t)(now->tv_sec - start->tv_sec) * 1000 + ((int64_t)now->tv_nsec - (int64_t)start->tv_nsec) / 1000000 ;
  return elapsed >= 0 && elapsed < (int64_t)ms ;
}

/** counted at an intrinsic run-then-die only (caller checks flagwantup): a death
 * inside the live window bumps the count, an expired window re-arms it to 1. A
 * zero MaxDeath disables the budget (infinite restart). */
static void record_death(void)
{
  if (!res.maxdeath) return ;
  struct timespec now ;
  clock_now_mono(&now) ;
  if (status.ndeaths && within_window(&status.window_start, &now, res.maxdeathtime))
    status.ndeaths++ ;
  else { status.window_start = now ; status.ndeaths = 1 ; }
}

/** budget exhausted: abandon the service, park the supervisor (FAILED/CRASH_LIMIT,
 * self-inflicted) and tell waiters it will not be restarted. A later wantup
 * (66 start/restart) clears flagfailed and resets the budget. */
static void enter_failed(void)
{
  status.flagfailed = 1 ;
  status.flagwantup = 0 ;
  status.result = STATUS_RESULT_CRASH_LIMIT ;
  latched_who = STATUS_WHO_SELF ;
  nextstart_set = 0 ;
  settimeout_infinite() ;
  clock_now(&status.stamp) ;
  announce() ;
  event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_NORESTART}, 1) ;
}

/** an exec that never ran ./run (spawn/pipe failure) is out of the crash budget:
 * retry with a progressive backoff (1->2->4->...->60s, doubling, capped) that
 * resets on the next successful exec. Carries EXEC_FAILED with the raw errno. */
static void exec_failed(int e)
{
  status.exec_errno = e ;
  status.result = STATUS_RESULT_EXEC_FAILED ;
  unsigned int delay = 1u << status.exec_backoff ;
  if (delay > 60) delay = 60 ;
  if (status.exec_backoff < 6) status.exec_backoff++ ;
  nextstart_set = 0 ;
  settimeout(delay) ;
  announce() ;
}

static void trystart(void)
{
  spawn_fa_t fa[2] ;
  char const *cargv[4] ;
  int notifyp[2] = { -1, -1 } ;
  unsigned int notif = res.notify ; // notification-fd, from the resolve (0 = none)
  uint16_t spawnflags = SPAWN_FLAG_SETSID ;
  uint8_t m = 0 ;

  // TODO flag-newpidns: posix_spawn cannot create a PID namespace.

  if (notif && notif <= maxfd)
  {
    if (pipe(notifyp) == -1)
    {
      int e = errno ;
      log_warnusys("create notification pipe") ;
      exec_failed(e) ;
      return ;
    }
    fa[0] = (spawn_fa_t){ .type = SPAWN_FA_CLOSE, .from = notifyp[0] } ;
    fa[1] = (spawn_fa_t){ .type = SPAWN_FA_MOVE, .from = notifyp[1], .to = (int)notif } ;
  }
  else if (notif)
    log_warn("invalid ", "fd for notification") ; // > maxfd: start without readiness

  cargv[m++] = "./run" ;
  cargv[m++] = servicename ;
  cargv[m++] = 0 ;
  status.pid = spawn_path_full(cargv[0], cargv, (char const *const *)environ, 0, 0, spawnflags, fa, notifyp[1] >= 0 ? 2 : 0) ;
  if (!status.pid)
  {
    int e = errno ;
    log_warnusys("spawn ", cargv[0]) ;
    exec_failed(e) ;
    goto errn ;
  }

  if (!sse_start_child(&g_epoll, &wchild, child_cb, NULL, status.pid, 1, true))
    log_dieusys(111, "watch child via pidfd") ;
  child_active = 1 ;

  if (notifyp[1] >= 0)
  {
    close_fd(notifyp[1]) ;
    notifyfd = notifyp[0] ;
    if (!sse_start_io(&g_epoll, &wnotify, notify_cb, NULL, notifyfd, SSE_READ, 0))
    {
      log_warnusys("watch notification pipe") ;
      close_fd(notifyfd) ;
      notifyfd = -1 ;
    }
    else notify_active = 1 ;
  }
  settimeout_infinite() ;
  nextstart_set = 0 ;
  state = UP ;
  status.flagready = 0 ;
  status.result = STATUS_RESULT_SUCCESS ;
  status.exec_backoff = 0 ; // ./run exec'd: clear the exec-failed backoff
  kill_timeout = 0 ;
  clock_now(&status.stamp) ;
  announce() ;
  event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_UP}, 1) ;
  return ;

 errn:
  if (notifyp[1] >= 0)
  {
    close_fd(notifyp[1]) ;
    close_fd(notifyp[0]) ;
  }
}

static void wantdown(void)
{
  if (status.flagwantup) latched_who = current_who ; // a real 1->0 owns the transition
  status.flagwantup = 0 ;
  status.flagfailed = 0 ; // a stop clears the failed latch -> plain DOWN
  announce() ;
}

static void wantup(void)
{
  if (!status.flagwantup)
  {
    latched_who = current_who ; // a real 0->1 owns the transition
    status.ndeaths = 0 ;        // an explicit start grants a fresh crash budget
    status.flagfailed = 0 ;     // and lifts the failed latch (reset-failed + start)
  }
  status.flagwantup = 1 ;
  announce() ;
}

static void wantDOWN(void)
{
  adddown() ;
  wantdown() ;
}

static void wantUP(void)
{
  deldown() ;
  wantup() ;
}

static void downtimeout(void)
{
  if (!status.flagwantup) { settimeout_infinite() ; return ; }
  // budget checked here: counter up to date (death already recorded), 1s throttle elapsed
  if (res.maxdeath && status.ndeaths >= res.maxdeath) enter_failed() ;
  else trystart() ;
}

static void down_o(void)
{
  wantdown() ;
  trystart() ;
}

static void down_u(void)
{
  wantup() ;
  trystart() ;
}

static void down_U(void)
{
  wantUP() ;
  trystart() ;
}

static int uplastup_z(void)
{
  char fmt0[16] ;
  char fmt1[16] ;
  char fmt2[24] ;
  char const *cargv[6] = { "finish", fmt0, fmt1, servicename, fmt2, 0 } ;

  status.flagpaused = 0 ;
  status.flagready = 0 ;
  gflags.dying = 0 ;

  /* a commanded stop (wantup cleared, we sent the down signal) is a clean
   * SUCCESS even though the process was signaled -- the who carries the intent.
   * Only an intrinsic death (still wanted up) is reported as SIGNALED/EXITED. */
  if (kill_timeout == 1) status.result = STATUS_RESULT_TIMEOUT_START ;
  else if (!status.flagwantup) status.result = STATUS_RESULT_SUCCESS ;
  else if (WIFSIGNALED(status.wstat)) status.result = STATUS_RESULT_SIGNALED ;
  else if (WEXITSTATUS(status.wstat)) status.result = STATUS_RESULT_EXITED ;
  else status.result = STATUS_RESULT_SUCCESS ;
  kill_timeout = 0 ;

  // an intrinsic death (still wanted up) is self-inflicted, whoever started it,
  // and is the only thing the crash budget counts (run-then-die)
  if (status.flagwantup) { latched_who = STATUS_WHO_SELF ; record_death() ; }

  clock_now(&status.stamp) ;
  drop_notifyfd() ;
  fmt0[u32_fmt(fmt0, WIFSIGNALED(status.wstat) ? 256 : WEXITSTATUS(status.wstat))] = 0 ;
  fmt1[u32_fmt(fmt1, WTERMSIG(status.wstat))] = 0 ;
  fmt2[pid_format(fmt2, status.pid)] = 0 ;

  status.pid = spawn_path_full("./finish", cargv, (char const *const *)environ, 0, 0, SPAWN_FLAG_SETSID, 0, 0) ;
  if (!status.pid)
  {
    if (errno != ENOENT) log_warnusys("spawn ", "./finish") ;
    set_down_and_ready((event_t[]){EVENT_DOWN, EVENT_DOWN_READY}, 2) ;
    return 0 ;
  }
  if (!sse_start_child(&g_epoll, &wchild, child_cb, NULL, status.pid, 1, true))
    log_dieusys(111, "watch finish child via pidfd") ;
  child_active = 1 ;
  {
    unsigned int timeout = res.execute.timeout.stop ? res.execute.timeout.stop : 5000 ;
    if (timeout) settimeout_ms(timeout) ;
    else settimeout_infinite() ;
  }
  status.flagfinishing = 1 ;
  announce() ;
  event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_DOWN}, 1) ;
  return 1 ;
}

static void up_z(void)
{
  if (uplastup_z()) state = FINISH ;
}

static void lastup_z(void)
{
  if (uplastup_z()) state = LASTFINISH ;
  else bail() ;
}

static void uptimeout(void)
{
  if (gflags.dying)
  {
    kill_timeout = 1 ;
    killk() ;
    settimeout(5) ;
  }
  else
  {
    settimeout_infinite() ;
    log_warn("can't happen: timeout while the service is up!") ;
  }
}

static void up_d(void)
{
  unsigned int timeout = res.execute.timeout.start ;   // timeout-kill (0 = infinite)
  if (status.flagwantup) latched_who = current_who ; // a real 1->0 owns the transition
  status.flagwantup = 0 ;
  killr() ;
  killc() ;
  if (timeout)
  {
    settimeout_ms(timeout) ;
    gflags.dying = 1 ;
  }
  else settimeout_infinite() ;
}

static void up_D(void)
{
  adddown() ;
  up_d() ;
}

static void up_x(void)
{
  state = LASTUP ;
  closethem() ;
}

static void up_term(void)
{
  state = LASTUP ;
  up_d() ;
}

static void finishtimeout(void)
{
  log_warn("finish script lifetime reached maximum value - sending it a SIGKILL") ;
  kill_timeout = 2 ;
  killc() ; killk() ;
  settimeout(5) ;
}

static void finish_z(void)
{
  if (kill_timeout == 2) status.result = STATUS_RESULT_TIMEOUT_STOP ;
  kill_timeout = 0 ;

  if (WIFEXITED(finish_wstat) && WEXITSTATUS(finish_wstat) == 125)
  {
    status.flagwantup = 0 ;
    set_down_and_ready((event_t[]){EVENT_NORESTART, EVENT_DOWN_READY}, 2) ;
  }
  else set_down_and_ready((event_t[]){EVENT_DOWN_READY}, 1) ;
}

static void finish_x(void)
{
  state = LASTFINISH ;
  closethem() ;
}

static void lastfinish_z(void)
{
  finish_z() ;
  bail() ;
}

static action_t_ref const actions[5][31] =
{
  { &downtimeout, &nop, &bail, &bail, &bail, &bail,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &down_o, &wantdown, &down_u, &wantDOWN, &down_U, &bail, &wantdown, &wantDOWN },
  { &uptimeout, &up_z, &up_term, &up_x, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr, &killl, &killP, &killC, &killK,
    &wantdown, &up_d, &wantup, &up_D, &wantUP, &up_x, &wantdown, &wantDOWN },
  { &finishtimeout, &finish_z, &finish_x, &finish_x, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &wantdown, &wantdown, &wantup, &wantDOWN, &wantUP, &finish_x, &wantdown, &wantDOWN },
  { &uptimeout, &lastup_z, &up_d, &closethem, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr, &killl, &killP, &killC, &killK,
    &wantdown, &up_d, &wantup, &up_D, &wantUP, &closethem, &wantdown, &wantDOWN },
  { &finishtimeout, &lastfinish_z, &nop, &closethem, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &wantdown, &wantdown, &wantup, &wantDOWN, &wantUP, &closethem, &wantdown, &wantDOWN }
} ;


/** 25-byte op alphabet (66 reload 'l' included). The bound is the full 25, so the
 * last command 'Q' (66-svctl -Q) is never dropped. Op bytes are letters/digits
 * (>= '0'), disjoint from the who bytes {0..5}, so the two never collide. */
static char const control_alphabet[] = "abqhkti12pcyrlPCKoduDUxOQ" ;

static void dispatch_op(char op, uint8_t who)
{
  char const *p = memchr(control_alphabet, op, 25) ;
  if (!p) return ;
  current_who = who ;
  (*actions[state][V_a + (size_t)(p - control_alphabet)])() ;
}

// control fifo: a fixed <op><who> pair per command (see SPECS)

static void control_cb(sse_watcher_t *w, void *data, int revents)
{
  log_flow() ;
  (void)w ; (void)data ;
  if (revents & (SSE_ERROR | SSE_HUP)) { log_warnusys("control watcher") ; return ; }
  for (;;)
  {
    char op ;

    if (pending_op >= 0) { op = (char)pending_op ; pending_op = -1 ; }
    else
    {
      ssize_t r = io_read_result(io_read(controlfd, &op, 1)) ;
      if (r < 0)
      {
        if (errno == EPIPE) break ; // EOF: never happens, we hold the write end
        log_dieusys(111, "read ", control_file) ;
      }
      if (!r) break ; // would block
      if (!memchr(control_alphabet, op, 25)) continue ; // parasite or orphan who: ignore
    }

    // op is a valid control byte; read its who.
    char wbyte ;
    ssize_t r = io_read_result(io_read(controlfd, &wbyte, 1)) ;
    if (r < 0)
    {
      if (errno == EPIPE) break ;
      log_dieusys(111, "read ", control_file) ;
    }
    if (!r) { pending_op = (unsigned char)op ; break ; } // who not here yet: buffer the op

    if ((unsigned char)wbyte < 6)
      dispatch_op(op, (uint8_t)wbyte) ;
    else
    {
      // a valid op not followed by a valid who (non-conformant writer): default
      // to USER and reinject wbyte so an orphan op does not eat the next one.
      dispatch_op(op, STATUS_WHO_USER) ;
      if (memchr(control_alphabet, wbyte, 25)) pending_op = (unsigned char)wbyte ;
    }
  }
}

static void signal_cb(sse_watcher_t *w, void *data, int revents)
{
  log_flow() ;
  (void)data ;
  if (revents & (SSE_ERROR | SSE_HUP)) { log_warnusys("signal watcher") ; gflags.cont = 0 ; return ; }
  switch ((int)((sse_signal_t *)w->sdata)->si.ssi_signo)
  {
    case SIGTERM : (*actions[state][V_TERM])() ; break ;
    case SIGHUP  : (*actions[state][V_HUP])() ; break ;
    case SIGQUIT : (*actions[state][V_QUIT])() ; break ;
    case SIGINT  : (*actions[state][V_INT])() ; break ;
    default : break ;
  }
}

static int trymkdir(char const *s)
{
  char buf[SUPERVISE_PATH_MAX] ;
  ssize_t r ;
  if (mkdir(s, 0700) >= 0) return 1 ;
  if (errno != EEXIST) log_dieusys(111, "mkdir ", s) ;
  r = readlink(s, buf, SUPERVISE_PATH_MAX) ;
  if (r < 0)
  {
    struct stat st ;
    if (errno != EINVAL)
    {
      errno = EEXIST ;
      log_dieusys(111, "mkdir ", s) ;
    }
    if (stat(s, &st) < 0)
      log_dieusys(111, "stat ", s) ;
    if (!S_ISDIR(st.st_mode))
      log_die(100, s, " exists and is not a directory") ;
    return 0 ;
  }
  else if (r == SUPERVISE_PATH_MAX)
  {
    errno = ENAMETOOLONG ;
    log_dieusys(111, "readlink ", s) ;
  }
  else
  {
    buf[r] = 0 ;
    if (mkdir(buf, 0700) < 0)
      log_dieusys(111, "mkdir ", buf) ;
    return 1 ;
  }
  return 0 ;
}

static inline int control_init(void)
{
  mode_t m = umask(0) ;
  int fdctl, fdlck, r ;
  if (!event_fifodir_make(SS_EVENTDIR + 1, getegid()))
    log_dieusys(111, "create event fifodir: ", SS_EVENTDIR + 1) ;

  trymkdir(SS_SUPERVISEDIR + 1) ;
  fdlck = io_open_mode(lock_file, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0644) ;
  if (fdlck < 0) log_dieusys(111, "open ", lock_file) ;
  r = fd_lock(fdlck, 1, 1) ;
  if (r < 0) log_dieusys(111, "lock ", lock_file) ;
  if (!r) log_die(100, "another instance of 66-supervise is already running") ;
 // fdlck leaks but it's coe

  if (mkfifo(control_file, 0600) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      log_dieusys(111, "mkfifo ", control_file) ;
    if (stat(control_file, &st) < 0)
      log_dieusys(111, "stat ", control_file) ;
    if (!S_ISFIFO(st.st_mode))
      log_die(100, control_file, " is not a FIFO") ;
  }
  fdctl = io_open(control_file, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
  if (fdctl < 0)
    log_dieusys(111, "open ", control_file, " for reading") ;
  r = io_open(control_file, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
  if (r < 0)
    log_dieusys(111, "open ", control_file, " for writing") ;
 // r leaks but it's coe

  umask(m) ;
  return fdctl ;
}

int main(int argc, char const *const *argv)
{
  resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

  PROG = "66-supervise" ;
  if (argc < 2) log_usage(USAGE) ;
  servicename = argv[1] ;

  /** scandir contract: 66-scandir runs us with CWD = scandir; chdir into
   * the service dir by name, then read the resolve locally from ./.resolve. */
  if (chdir(servicename) < 0)
    log_dieusys(111, "chdir to ", servicename) ;

  if (!resolve_read(wres, ".", servicename)) {
    log_dieusys(111, "read resolve file of: ", servicename) ;
  }

  {
    size_t proglen = strlen(PROG) ;
    size_t namelen = strlen(argv[1]) ;
    char progname[proglen + namelen + 2] ;
    memcpy(progname, PROG, proglen) ;
    progname[proglen] = ' ' ;
    memcpy(progname + proglen + 1, argv[1], namelen + 1) ;
    PROG = progname ;
    if (!ensure_stdfds())
      log_dieusys(111, "sanitize stdin and stdout") ;
    {
      struct rlimit rl ;
      if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
        log_dieusys(111, "getrlimit") ;
      maxfd = rl.rlim_cur ;
    }

    if (!sse_new(&g_epoll, 8))
      log_dieusys(111, "create event loop") ;

    auto_strings(status_file, SS_SUPERVISEDIR + 1, "/", SS_STATUS) ;
    auto_strings(control_file, SS_SUPERVISEDIR + 1, "/", SS_CONTROL) ;
    auto_strings(lock_file, SS_SUPERVISEDIR + 1, "/", SS_LOCK) ;

    controlfd = control_init() ;

    if (!sse_start_signal(&g_epoll, &wsignal, signal_cb, NULL, 1))
      log_dieusys(111, "init signal watcher") ;
    if (!sse_ignore_signal(&wsignal, SIGPIPE)
     || !sse_attach_signal(&wsignal, SIGTERM)
     || !sse_attach_signal(&wsignal, SIGHUP)
     || !sse_attach_signal(&wsignal, SIGQUIT)
     || !sse_attach_signal(&wsignal, SIGINT))
      log_dieusys(111, "trap signals") ;

    if (!sse_start_io(&g_epoll, &wcontrol, control_cb, NULL, controlfd, SSE_READ, 0))
      log_dieusys(111, "watch control fifo") ;

    if (!event_fifodir_clean(SS_EVENTDIR + 1))
      log_warnusys("clean ", SS_EVENTDIR + 1) ;

    if (access("down", F_OK) == 0) status.flagwantup = 0 ; // latched_who already defaults to SELF
    else if (errno != ENOENT)
      log_dieusys(111, "access ./down") ;

    settimeout(0) ;
    clock_now(&status.stamp) ;
    status.readystamp = status.stamp ;
    announce() ;
    event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_SUPERVISE_UP}, 1) ;

    g_epoll.running = true ;
    while (gflags.cont)
    {
      do { sse_sanitize(&g_epoll) ; } while (g_epoll.rerun_file) ;
      if (!sse_prepare(&g_epoll)) log_warnusys("prepare watchers") ;

      int nfds = sse_wait(&g_epoll, compute_timeout()) ;
      if (nfds < 0) log_dieusys(111, "sse wait") ;
      else if (!nfds) (*actions[state][V_TIMEOUT])() ;
      else sse_dispatch(&g_epoll) ;

      // deferred teardown, in main-loop context (never from inside dispatch)
      if (child_died)
      {
        int wstat = child_wstat ;
        child_died = 0 ;
        if (child_active) { sse_free_child(&wchild) ; child_active = 0 ; }
        if (status.flagfinishing) finish_wstat = wstat ;
        else status.wstat = wstat ;
        (*actions[state][V_CHLD])() ;
      }
      if (notify_drop_req) { notify_drop_req = 0 ; drop_notifyfd() ; }
    }

    event_fifodir_emit(SS_EVENTDIR + 1, (event_t[]){EVENT_SUPERVISE_DOWN}, 1) ;
  }
  resolve_free(wres) ;
  return 0 ;
}
