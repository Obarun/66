/* 
 * 66-umountall.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a strict copy of s6-linux-init-umountall.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */
 
#include <string.h>
#include <sys/mount.h>

#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#define BUFSIZE 4096
#define MAXLINES 512

int main (int argc, char const *const *argv)
{
  stralloc mountpoints[MAXLINES] ;
  char buf[BUFSIZE] ;
  buffer b ;
  stralloc sa = STRALLOC_ZERO ;
  unsigned int line = 0 ;
  int e = 0 ;
  int r ;
  int fd ;

  PROG = "s6-linux-init-umountall" ;


 /*
    We need to go through /proc/mounts *in reverse order*, because later mounts
    may depend on earlier mounts.
    The getmntent() family of functions has obviously not been designed for that
    use case at all, and it is actually more difficult to use it than to do the
    /proc/mounts parsing by hand. So, we do it by hand with skalibs functions.
    That's how you can tell a good API from a terrible one.
 */

  fd = open_readb("/proc/mounts") ;
  if (fd < 0) strerr_diefu1sys(111, "open /proc/mounts") ;
  memset(mountpoints, 0, sizeof(mountpoints)) ;
  buffer_init(&b, &buffer_read, fd, buf, BUFSIZE) ;
  for (;;)
  {
    size_t n, p ;
    if (line >= MAXLINES) strerr_dief1x(111, "/proc/mounts too big") ;
    sa.len = 0 ;
    r = skagetln(&b, &sa, '\n') ;
    if (r <= 0) break ;
    p = byte_chr(sa.s, sa.len, ' ') ;
    if (p >= sa.len) strerr_dief1x(111, "bad /proc/mounts format") ;
    p++ ;
    n = byte_chr(sa.s + p, sa.len - p, ' ') ;
    if (n == sa.len - p) strerr_dief1x(111, "bad /proc/mounts format") ;
    if (!stralloc_catb(&mountpoints[line], sa.s + p, n) || !stralloc_0(&mountpoints[line]))
      strerr_diefu1sys(111, "store mount point") ;
    line++ ;
  }
  fd_close(fd) ;
  stralloc_free(&sa) ;
  if (r < 0) strerr_diefu1sys(111, "read /proc/mounts") ;

  while (line--)
    if (umount(mountpoints[line].s) == -1)
    {
      e++ ;
      strerr_warnwu2sys("umount ", mountpoints[line].s) ;
    }
  return e ;
}
