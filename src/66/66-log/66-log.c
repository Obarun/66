/*
 * 66-log.c
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
 * 66's own logging daemon, oblibs-native. It descends from skarnet's s6-log but
 * is deliberately specialized for the only way 66 ever drives a logger (the boot
 * catch-all and the per-service logger): timestamp a stream of lines and append
 * them to ONE log directory, with size-based rotation and archive trimming,
 * optionally echoing them to fd 1. Everything in s6-log that 66 never generates
 * is gone: selection regexes, rotation processors, status/alert/prefix actions,
 * multiple log directories, time/dir-size rotation, and the q/v/l/t options. A
 * frontend that needs those keeps Build=custom and runs s6-log directly.
 *
 * What is kept verbatim from the s6-log contract: the TAI64N archive names and
 * line timestamps, the size-rotation policy, the .u/.s crash-recovery markers,
 * the non-blocking buffering, the -d/-b/-p options, and the exit codes. The
 * event loop is a plain poll() over a signalfd, stdin and (when there is pending
 * output) stdout, exactly like s6-log's iopause loop.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include <oblibs/clock.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/rbuffer.h>
#include <oblibs/stream.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#define LINELIMIT 8192 // s6-log default; 66 never overrides it
#define LASTLINE_MS 2000 // last-line grace after SIGTERM/SIGHUP
#define RETRY_SEC 2 // backoff after a failed write/rotation step
#define LOG_STAMP CLOCK_TAI64N_LEN // '@' + 24 hex
#define LOGBUF_SIZE 8192 // the log directory's output buffer

// Monotonic deadlines, with an explicit "infinite" flag (skalibs TAIN_INFINITE).

typedef struct deadline_s deadline_t ;
struct deadline_s { struct timespec t ; int inf ; } ;

#define DEADLINE_INFINITE { .t = { 0, 0 }, .inf = 1 }

static void deadline_from_now_sec(deadline_t *d, int64_t sec)
{
  struct timespec now ;
  clock_now_mono(&now) ;
  clock_addsec(&d->t, &now, sec) ;
  d->inf = 0 ;
}

static void deadline_from_now_ms(deadline_t *d, uint32_t ms)
{
  struct timespec now, rel ;
  clock_now_mono(&now) ;
  clock_from_ms(&rel, ms) ;
  clock_add(&d->t, &now, &rel) ;
  d->inf = 0 ;
}

static inline void deadline_set_infinite(deadline_t *d) { d->inf = 1 ; }

static void deadline_min(deadline_t *acc, deadline_t const *x)
{
  if (x->inf) return ;
  if (acc->inf || clock_cmp(&x->t, &acc->t) < 0) *acc = *x ;
}

static int deadline_future(deadline_t const *d)
{
  if (d->inf) return 1 ;
  struct timespec now ;
  clock_now_mono(&now) ;
  return clock_cmp(&d->t, &now) > 0 ;
}

static int compute_timeout(deadline_t const *d)
{
  if (d->inf) return -1 ;
  struct timespec now ;
  clock_now_mono(&now) ;
  int64_t ms = (int64_t)(d->t.tv_sec - now.tv_sec) * 1000 + ((int64_t)d->t.tv_nsec - (int64_t)now.tv_nsec) / 1000000 ;
  if (ms < 0) ms = 0 ;
  if (ms > INT_MAX) ms = INT_MAX ;
  return (int)ms ;
}

// strict unsigned scan (mirrors skalibs uint0_scan): whole string must be the number
static int uint32_scan0(char const *s, uint32_t *u)
{
  size_t n = u32_scan(s, u) ;
  return n && !s[n] ;
}

/** Is @s a syntactically valid TAI64N external stamp ('@' + 24 hex)? Read-side
 * counterpart of clock_tai64n_fmt; 66-log only needs to recognize its own
 * archive names, never to decode them. Reads CLOCK_TAI64N_LEN bytes of @s. */
static int tai64n_is_valid(char const *s)
{
  if (s[0] != '@') return 0 ;
  for (unsigned int i = 1 ; i < CLOCK_TAI64N_LEN ; i++)
  {
    char c = s[i] ;
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return 0 ;
  }
  return 1 ;
}

/** Offset, across the @n buffered regions of @v, of the first byte that is one of
 * the @numbytes bytes of @bytes; the total length if none. Used to find a line
 * terminator in the (possibly wrapped) istream ring. */
static size_t siovec_bytein(struct iovec const *v, unsigned int n, char const *bytes, size_t numbytes)
{
  size_t pos = 0 ;
  for (unsigned int i = 0 ; i < n ; i++) {
    char const *s = v[i].iov_base ;
    size_t len = v[i].iov_len, best = len ;
    for (size_t k = 0 ; k < numbytes ; k++) {
      char const *p = memchr(s, bytes[k], best) ;
      if (p) best = (size_t)(p - s) ;
    }
    if (best < len) return pos + best ;
    pos += len ;
  }
  return pos ;
}

// Globals

static mode_t mask ;
static int flagprotect = 0 ;
static int flagexiting = 0 ;
static unsigned int gflags = 0 ;        // 1 = TAI64N line stamp, 2 = local ISO stamp
static int to_fd1 = 0 ;                 // echo every line to fd 1 (ostream_1)
static int sfd = -1 ;                   // signalfd
static deadline_t exit_deadline = DEADLINE_INFINITE ;

static strbuf indata = STRBUF_ZERO ;

/** The one log directory. Its current file is a BLOCKING fd, so its ostream
 * always drains fully in one flush; rotation is triggered afterwards from the
 * byte counter rather than from inside the writer. */

typedef enum rotstate_e rotstate_t ;
enum rotstate_e
{
  ROTSTATE_WRITABLE,
  ROTSTATE_FLUSHING,
  ROTSTATE_START,
  ROTSTATE_RENAME,
  ROTSTATE_NEWCURRENT,
  ROTSTATE_CHMODPREVIOUS,
  ROTSTATE_FINISHPREVIOUS,
  ROTSTATE_ENDFCHMOD,
  ROTSTATE_END
} ;

typedef struct logdir_s logdir_t ;
struct logdir_s
{
  ostream os ;           // buffered writer over the current file
  deadline_t retrydeadline ;
  uint64_t b ;           // bytes appended to the current file since last rotation
  uint32_t s ;           // max file size before rotation
  uint32_t n ;           // archives to keep
  uint32_t tolerance ;   // rotation slack
  char const *dir ;
  int fd ;
  rotstate_t rstate ;
} ;

static logdir_t ld ;
static int have_logdir = 0 ;
static char logbuf[LOGBUF_SIZE] ;

struct filedesc_s { char name[28] ; } ; // archive name, sorted to drop the oldest

typedef int qcmp_func(void const *, void const *) ;

static int rotator(void) ;
static void process_partial_line(void) ;
static void normal_stdin(void) ;
static void (*handle_stdin)(void) = &normal_stdin ;

static inline size_t logdir_pending(void) { return rbuffer_used(&ld.os.rb) ; }
static inline size_t fd1_pending(void) { return rbuffer_used(&ostream_1->rb) ; }

// Log directory machinery

static int filedesc_cmp(struct filedesc_s const *a, struct filedesc_s const *b)
{
  return memcmp(a->name + 1, b->name + 1, 26) ;
}

static int name_is_relevant(char const *name)
{
  if (strlen(name) != 27) return 0 ;
  if (!tai64n_is_valid(name)) return 0 ;
  if (name[25] != '.') return 0 ;
  if ((name[26] != 's') && (name[26] != 'u')) return 0 ;
  return 1 ;
}

// keep at most ld.n archives
static inline int logdir_trim(void)
{
  unsigned int n = 0 ;
  DIR *dir = opendir(ld.dir) ;
  if (!dir) return -1 ;
  for (;;)
  {
    struct dirent *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    if (name_is_relevant(d->d_name)) n++ ;
  }
  if (errno) { closedir(dir) ; return -1 ; }
  if (!n) { closedir(dir) ; return 0 ; }

  rewinddir(dir) ;
  {
    size_t dirlen = strlen(ld.dir) ;
    unsigned int i = 0 ;
    struct filedesc_s archive[n] ;
    char fullname[dirlen + 29] ;
    memcpy(fullname, ld.dir, dirlen) ;
    fullname[dirlen] = '/' ;
    for (;;)
    {
      struct dirent *d ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (!name_is_relevant(d->d_name)) continue ;
      if (i >= n) { errno = EBUSY ; break ; }
      memcpy(archive[i].name, d->d_name, 28) ;
      i++ ;
    }
    if (errno) { closedir(dir) ; return -1 ; }
    closedir(dir) ;
    if (i <= ld.n) return 0 ;
    qsort(archive, i, sizeof(struct filedesc_s), (qcmp_func *)&filedesc_cmp) ;
    n = 0 ;
    while (i > ld.n + n)
    {
      memcpy(fullname + dirlen + 1, archive[n].name, 28) ;
      if (unlink(fullname) < 0) log_warnusys("unlink ", fullname) ;
      n++ ;
    }
  }
  return n ;
}

static void logdir_set_writable(void)
{
  ld.rstate = ROTSTATE_WRITABLE ;
  deadline_set_infinite(&ld.retrydeadline) ;
}

// archive @name as @suffix (.s finished, .u unfinished), then trim
static int finish(char const *name, char suffix)
{
  struct stat st ;
  size_t dirlen = strlen(ld.dir) ;
  size_t namelen = strlen(name) ;
  char x[dirlen + namelen + 2] ;
  memcpy(x, ld.dir, dirlen) ;
  x[dirlen] = '/' ;
  memcpy(x + dirlen + 1, name, namelen + 1) ;
  if (stat(x, &st) < 0) return errno == ENOENT ? 0 : -1 ;
  if (st.st_nlink == 1 && st.st_size)
  {
    struct timespec now ;
    char y[dirlen + 29] ;
    memcpy(y, ld.dir, dirlen) ;
    y[dirlen] = '/' ;
    clock_now(&now) ;
    clock_tai64n_fmt(y + dirlen + 1, &now) ;
    y[dirlen + 26] = '.' ;
    y[dirlen + 27] = suffix ;
    y[dirlen + 28] = 0 ;
    if (link(x, y) < 0) return -1 ;
  }
  if (unlink(x) < 0) return -1 ;
  return logdir_trim() ;
}

/** the synchronous rotation state machine (no processor: each step either
 * completes or fails into a timed retry). The output buffer is always drained
 * before rotation is entered, so the ostream just follows the new fd. */
static int rotator(void)
{
  size_t dirlen = strlen(ld.dir) ;
  switch (ld.rstate)
  {
    case ROTSTATE_START :
      if (fsync(ld.fd) < 0) { log_warnusys("fsync ", ld.dir, "/current") ; goto fail ; }
      ld.rstate = ROTSTATE_RENAME ;
      // fallthrough
    case ROTSTATE_RENAME :
    {
      char current[dirlen + 9] ;
      char previous[dirlen + 10] ;
      memcpy(current, ld.dir, dirlen) ; memcpy(current + dirlen, "/current", 9) ;
      memcpy(previous, ld.dir, dirlen) ; memcpy(previous + dirlen, "/previous", 10) ;
      if (rename(current, previous) < 0) { log_warnusys("rename ", current, " to ", previous) ; goto fail ; }
      ld.rstate = ROTSTATE_NEWCURRENT ;
    }
    // fallthrough
    case ROTSTATE_NEWCURRENT :
    {
      char x[dirlen + 9] ;
      memcpy(x, ld.dir, dirlen) ; memcpy(x + dirlen, "/current", 9) ;
      int fd = io_open_mode(x, O_WRONLY | O_NONBLOCK | O_APPEND | O_CREAT | O_CLOEXEC, 0666) ;
      if (fd < 0) { log_warnusys("open_append ", x) ; goto fail ; }
      if (!io_set_block(fd)) { log_warnusys("ndelay_off ", x) ; close_fd(fd) ; goto fail ; }
      close_fd(ld.fd) ;
      ld.fd = fd ;
      ld.os.fd = fd ;        // the drained buffer follows the new current file
      ld.b = 0 ;
      ld.rstate = ROTSTATE_CHMODPREVIOUS ;
    }
    // fallthrough
    case ROTSTATE_CHMODPREVIOUS :
    {
      char x[dirlen + 10] ;
      memcpy(x, ld.dir, dirlen) ; memcpy(x + dirlen, "/previous", 10) ;
      if (chmod(x, mask | S_IXUSR) < 0) { log_warnusys("chmod ", x) ; goto fail ; }
      ld.rstate = ROTSTATE_FINISHPREVIOUS ;
    }
    // fallthrough
    case ROTSTATE_FINISHPREVIOUS :
      if (finish("previous", 's') < 0) { log_warnusys("finish previous .s to logdir ", ld.dir) ; goto fail ; }
      logdir_set_writable() ;
      break ;
    default : log_die(101, "inconsistent state in rotator()") ;
  }
  return 1 ;
 fail:
  deadline_from_now_sec(&ld.retrydeadline, RETRY_SEC) ;
  return 0 ;
}

// drain the buffer to the current file (blocking) and rotate past the size cap
static void logdir_flush(void)
{
  if (!ostream_flush(&ld.os))
  {
    ld.rstate = ROTSTATE_FLUSHING ;
    deadline_from_now_sec(&ld.retrydeadline, RETRY_SEC) ;
    return ;
  }
  logdir_set_writable() ;
  if (ld.b + ld.tolerance >= ld.s) { ld.rstate = ROTSTATE_START ; rotator() ; }
}

static void logdir_rotate(void)
{
  /** best-effort flush so the buffered lines land in the archived file; a
   * remainder (write error) simply carries over to the new current. Rotate
   * unconditionally: a forced rotation (SIGALRM) must not depend on the flush. */
  if (logdir_pending()) ostream_flush(&ld.os) ;
  ld.rstate = ROTSTATE_START ;
  rotator() ;
}

static void logdir_retry(void)
{
  if (ld.rstate == ROTSTATE_FLUSHING) logdir_flush() ;
  else rotator() ;
}

static inline void logdir_init(uint32_t s, uint32_t n, char const *name)
{
  struct stat st ;
  size_t dirlen = strlen(name) ;
  int r ;
  char x[dirlen + 11] ;
  ld.s = s ;
  ld.n = n ;
  ld.tolerance = 2000 ;
  ld.dir = name ;
  ld.fd = -1 ;
  r = mkdir(ld.dir, S_IRWXU | S_ISGID) ;
  if (r < 0 && errno != EEXIST) log_dieusys(111, "mkdir ", name) ;
  memcpy(x, name, dirlen) ;
  memcpy(x + dirlen, "/lock", 6) ;
  // the lock is held by this fd for the whole run; it is never closed (released
  // at exit), so it needs no home in the logdir struct
  int fdlock = io_open_mode(x, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0666) ;
  if (fdlock < 0) log_dieusys(111, "open ", x) ;
  r = lock_fd(fdlock, 1, 1) ;
  if (!r) errno = EBUSY ;
  if (r < 1) log_dieusys(111, "lock ", x) ;
  memcpy(x + dirlen + 1, "current", 8) ;
  if (stat(x, &st) < 0)
  {
    if (errno != ENOENT) log_dieusys(111, "stat ", x) ;
  }
  else if (st.st_mode & S_IXUSR) goto opencurrent ; // cleanly closed: reuse it
  /** an unclean current (or a leftover previous from an interrupted rotation):
   * archive both as unfinished, then start a fresh current */
  if (finish("previous", 'u') < 0) log_dieusys(111, "finish previous .u for logdir ", ld.dir) ;
  if (finish("current", 'u') < 0) log_dieusys(111, "finish current .u for logdir ", ld.dir) ;
  st.st_size = 0 ;
  memcpy(x + dirlen + 1, "current", 8) ;
 opencurrent:
  ld.fd = io_open_mode(x, O_WRONLY | O_NONBLOCK | O_APPEND | O_CREAT | O_CLOEXEC, 0666) ;
  if (ld.fd < 0) log_dieusys(111, "open_append ", x) ;
  if (!io_set_block(ld.fd)) log_dieusys(111, "ndelay_off ", x) ;
  if (fchmod(ld.fd, mask) == -1) log_dieusys(111, "fchmod ", x) ;
  ld.b = st.st_size ;
  if (!ostream_init(&ld.os, ld.fd, logbuf, sizeof logbuf)) log_dieusys(111, "init log buffer") ;
  logdir_set_writable() ;
  have_logdir = 1 ;
}

static inline int logdir_finalize(void)
{
  switch (ld.rstate)
  {
    case ROTSTATE_WRITABLE :
      if (fsync(ld.fd) < 0) { log_warnusys("fsync ", ld.dir, "/current") ; goto fail ; }
      ld.rstate = ROTSTATE_ENDFCHMOD ;
      // fallthrough
    case ROTSTATE_ENDFCHMOD :
      if (fchmod(ld.fd, mask | S_IXUSR) < 0) { log_warnusys("fchmod ", ld.dir, "/current") ; goto fail ; }
      ld.rstate = ROTSTATE_END ;
      break ;
    default : log_die(101, "inconsistent state in logdir_finalize()") ;
  }
  return 1 ;
 fail:
  deadline_from_now_sec(&ld.retrydeadline, RETRY_SEC) ;
  return 0 ;
}

// on exit, mark the current file cleanly closed (the x bit), retrying on error
static inline void finalize(void)
{
  if (!have_logdir) return ;
  for (;;)
  {
    deadline_t deadline ;
    deadline_from_now_sec(&deadline, 2) ;
    if (ld.rstate == ROTSTATE_END) break ;
    if (logdir_finalize()) break ;
    deadline_min(&deadline, &ld.retrydeadline) ;
    clock_deepsleep(&deadline.t) ;
  }
}

// fd 1 echo died (write error or unrecoverable backpressure): stop echoing
static void close_fd1_echo(void)
{
  to_fd1 = 0 ;
}

static void line_out(char const *s, size_t len)
{
  struct iovec v[4] ; // at most: TAI64N stamp, local stamp, line, '\n'
  unsigned int m = 0 ;
  size_t total = 0 ;
  char tstamp[LOG_STAMP + 1] ;
  char hstamp[CLOCK_LOCAL_LEN + 2] ;
  if (gflags & 3)
  {
    struct timespec now ;
    clock_now(&now) ;
    if (gflags & 1)
    {
      clock_tai64n_fmt(tstamp, &now) ;
      tstamp[LOG_STAMP] = ' ' ;
      v[m].iov_base = tstamp ;
      v[m++].iov_len = LOG_STAMP + 1 ;
    }
    if (gflags & 2)
    {
      /** clock_local_fmt returns the would-be length: it can exceed (and the
       * output is truncated to) CLOCK_LOCAL_LEN for a >= 5-digit year */
      size_t hlen = clock_local_fmt(hstamp, &now) ;
      if (hlen > CLOCK_LOCAL_LEN) hlen = CLOCK_LOCAL_LEN ;
      hstamp[hlen++] = ' ' ;
      hstamp[hlen++] = ' ' ;
      v[m].iov_base = hstamp ;
      v[m++].iov_len = hlen ;
    }
  }
  v[m].iov_base = (char *)s ;
  v[m++].iov_len = len ;
  v[m].iov_base = "\n" ;
  v[m++].iov_len = 1 ;
  for (unsigned int i = 0 ; i < m ; i++) total += v[i].iov_len ;

  /** fd 1 is best-effort: if it backs up past its buffer (slow reader) or errors,
   * drop the echo rather than stall the log directory */
  if (to_fd1 && !ostream_putv(ostream_1, v, m)) close_fd1_echo() ;

  if (have_logdir)
  {
    ld.b += total ;
    if (!ostream_putv(&ld.os, v, m))
    {
      ld.rstate = ROTSTATE_FLUSHING ;
      deadline_from_now_sec(&ld.retrydeadline, RETRY_SEC) ;
    }
  }
}

static void prepare_to_exit(void)
{
  // stop polling stdin; the loop breaks once the buffered output has drained
  flagexiting = 1 ;
}

static inline int getchunk(strbuf *sa, size_t linelimit)
{
  struct iovec v[2] ;
  size_t pos ;
  int r ;
  unsigned int nv = rbuffer_riovec(&istream_0->rb, v) ;
  size_t buflen = rbuffer_used(&istream_0->rb) ;
  pos = siovec_bytein(v, nv, "\n", 2) ;
  if (linelimit && sa->len + pos > linelimit)
  {
    r = 2 ;
    pos = linelimit - sa->len ;
  }
  else
  {
    r = pos < buflen ;
    pos += r ;
  }
  if (!strbuf_reserve(sa, sa->len + pos + (r == 2))) return -1 ;
  istream_getnofill(istream_0, sa->s + sa->len, pos) ; sa->len += pos ;
  if (r == 2) sa->s[sa->len++] = 0 ;
  return r ;
}

static void normal_stdin(void)
{
  ssize_t r = istream_fill(istream_0) ;
  if (r < 0)
  {
    if (error_isagain(errno)) { errno = 0 ; return ; }
    if (errno != EPIPE) log_warnusys("read from stdin") ;
    prepare_to_exit() ;
    return ;
  }
  if (!r) { prepare_to_exit() ; return ; } // EOF
  for (;;)
  {
    r = getchunk(&indata, LINELIMIT) ;
    if (r < 0) log_die_nomem("buffer") ;
    else if (!r) break ;
    line_out(indata.s, indata.len - 1) ; // len-1: drop the trailing '\n'/NUL getchunk appended
    indata.len = 0 ;
  }
}

static void process_partial_line(void)
{
  /** a partial line carries no trailing '\n'; line_out takes an explicit length,
   * so no NUL terminator is needed */
  line_out(indata.s, indata.len) ;
  indata.len = 0 ;
}

static void last_stdin(void)
{
  for (;;)
  {
    char c ;
    ssize_t rd = io_read(0, &c, 1) ;
    if (rd < 0 && error_isagain(errno)) { errno = 0 ; rd = 0 ; }
    switch (rd)
    {
      case 0 : return ;
      case -1 :
        if (errno != EPIPE) log_warnusys("read from stdin") ;
        if (indata.len) goto lastline ;
        goto end ;
      default :
        if (c == '\n' || !c) goto lastline ;
        if (!strbuf_catb(&indata, &c, 1)) log_die_nomem("buffer") ;
        if (indata.len >= LINELIMIT)
        {
          log_warn("input line too long, ", "stopping before the end") ;
          goto lastline ;
        }
        else break ;
    }
  }
 lastline:
  process_partial_line() ;
 end:
  prepare_to_exit() ;
}

// Signals: SIGTERM/SIGHUP end the stream, SIGALRM forces a rotation

static void handle_signals(void)
{
  for (;;)
  {
    struct signalfd_siginfo si ;
    ssize_t r = io_read(sfd, (char *)&si, sizeof(si)) ;
    if (r < (ssize_t)sizeof(si)) break ;
    switch ((int)si.ssi_signo)
    {
      case SIGALRM :
        if (have_logdir) logdir_rotate() ;
        break ;
      case SIGTERM :
        if (flagprotect) break ;
        // fallthrough
      case SIGHUP :
        handle_stdin = &last_stdin ;
        deadline_from_now_ms(&exit_deadline, LASTLINE_MS) ;
        if (!indata.len) prepare_to_exit() ;
        break ;
      default : break ;
    }
  }
}

// Main

static opt_t const opts[] =
{
  { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
  { .id = 'b', .shortname = 'b', .arg = OPT_NONE, .help = "block instead of dropping when output is full" },
  { .id = 'p', .shortname = 'p', .arg = OPT_NONE, .help = "protect against SIGTERM" },
  { .id = 'd', .shortname = 'd', .arg = OPT_REQUIRED, .argname = "fd", .help = "write a newline to fd when ready" }
} ;

static opt_cmd_t const cmd =
{
  .name = "66-log",
  .operands = "[ n[N] | s[N] | t | T | 1 ]... logdir",
  .epilog =
    "Logging directives, after the options (any order):\n"
    "  n[N]   keep N rotated archives (default 10)\n"
    "  s[N]   rotate the current file once it passes N bytes (default 99999)\n"
    "  t      prefix each line with a TAI64N timestamp\n"
    "  T      prefix each line with a local ISO 8601 timestamp\n"
    "  1      also echo each line to stdout\n"
    "  logdir the log directory (mandatory unless 1 is given)",
  .opts = opts,
  .nopts = OPT_COUNT(opts),
} ;

int main(int argc, char const *const *argv)
{
  unsigned int notif = 0 ;
  int flagblock = 0 ;
  uint32_t s = 99999 ;
  uint32_t n = 10 ;
  char const *logdir = 0 ;
  PROG = "66-log" ;
  VERBOSITY = LOG_LEVEL_WARN ;

  {
    opt_scan_t st = OPT_SCAN_ZERO ;
    for (;;)
    {
      int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
      if (o == OPT_END) break ;
      switch (o)
      {
        case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
        case 'b' : flagblock = 1 ; break ;
        case 'p' : flagprotect = 1 ; break ;
        case 'd' :
          if (!uint32_scan0(st.arg, &notif)) return opt_emit_usage(cmd.name, &cmd) ;
          if (notif < 3) log_die(100, "notification fd must be 3 or more") ;
          if (fcntl(notif, F_GETFD) < 0) log_dieusys(100, "invalid notification fd") ;
          break ;
        default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
      }
    }
    argc -= st.ind ; argv += st.ind ;
  }

  // the logging script: a flat list of directives, at most one log directory
  for (; *argv ; argv++)
  {
    switch (**argv)
    {
      case 'n' :
        if ((*argv)[1] && !uint32_scan0(*argv + 1, &n)) log_die(100, "invalid directive: ", *argv) ;
        break ;
      case 's' :
        if ((*argv)[1] && !uint32_scan0(*argv + 1, &s)) log_die(100, "invalid directive: ", *argv) ;
        if (s < 4096) s = 4096 ;
        if (s > 268435455) s = 268435455 ;
        break ;
      case 't' : if ((*argv)[1]) log_die(100, "invalid directive: ", *argv) ; gflags |= 1 ; break ;
      case 'T' : if ((*argv)[1]) log_die(100, "invalid directive: ", *argv) ; gflags |= 2 ; break ;
      case '1' : if ((*argv)[1]) log_die(100, "invalid directive: ", *argv) ; to_fd1 = 1 ; break ;
      case '.' :
      case '/' :
        if (logdir) log_die(100, "66-log drives a single log directory") ;
        logdir = *argv ;
        break ;
      default : log_die(100, "unrecognized directive: ", *argv) ;
    }
  }

  if (!logdir && !to_fd1) return opt_emit_usage(cmd.name, &cmd) ;

  if (!ensure_stdfds()) log_dieusys(111, "ensure stdin/stdout/stderr are open") ;
  if (!io_set_nonblock(0)) log_dieusys(111, "set stdin non-blocking") ;
  if (to_fd1 && !io_set_nonblock(1)) log_dieusys(111, "set stdout non-blocking") ;
  mask = umask(0) ;
  umask(mask) ;
  mask = ~mask & 0666 ;

  if (logdir) logdir_init(s, n, logdir) ;

  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGHUP) ;
    sigaddset(&set, SIGALRM) ;
    if (sigprocmask(SIG_BLOCK, &set, 0) < 0) log_dieusys(111, "block signals") ;
    sfd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC) ;
    if (sfd < 0) log_dieusys(111, "signalfd") ;
    signal(SIGPIPE, SIG_IGN) ;
  }

  if (notif)
  {
    io_write(notif, "\n", 1) ;
    close_fd(notif) ;
  }

  for (;;)
  {
    deadline_t deadline = exit_deadline ;
    int r = 0 ;
    struct pollfd x[3] ;
    nfds_t j = 0 ;
    int ising, isout = -1, isin = -1 ;

    x[j].fd = sfd ; x[j].events = POLLIN ; ising = (int)j++ ;

    if (to_fd1 && fd1_pending())
    {
      r = 1 ;
      x[j].fd = 1 ; x[j].events = POLLOUT ; isout = (int)j++ ;
    }

    if (have_logdir)
    {
      if (ld.rstate == ROTSTATE_WRITABLE && logdir_pending()) logdir_flush() ;
      if (ld.rstate != ROTSTATE_WRITABLE || logdir_pending()) r = 1 ;
      deadline_min(&deadline, &ld.retrydeadline) ;
    }

    if (!flagexiting && !(flagblock && r))
    {
      x[j].fd = 0 ; x[j].events = POLLIN ; isin = (int)j++ ;
    }

    if (flagexiting && !r) break ;

    int nr = poll(x, j, compute_timeout(&deadline)) ;
    if (nr < 0)
    {
      if (errno == EINTR) continue ;
      log_dieusys(111, "poll") ;
    }
    if (!nr)
    {
      if (!deadline_future(&exit_deadline))
      {
        if (indata.len) process_partial_line() ;
        prepare_to_exit() ;
      }
      if (have_logdir && !deadline_future(&ld.retrydeadline)) logdir_retry() ;
      continue ;
    }

    if (x[ising].revents & POLLIN) handle_signals() ;

    if (isout >= 0 && x[isout].revents)
    {
      errno = 0 ;
      if (!ostream_flush(ostream_1) && errno) close_fd1_echo() ;
    }

    if (isin >= 0 && x[isin].revents)
    {
      if (x[isin].revents & POLLIN) (*handle_stdin)() ;
      else
      {
        if (indata.len) process_partial_line() ;
        prepare_to_exit() ;
      }
    }
  }
  finalize() ;
  return 0 ;
}
