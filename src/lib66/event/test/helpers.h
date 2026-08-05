/* helpers.h — test scaffolding shared by the event test files.
 *
 * fanout_*: mimic 66-supervise's fanout. The real producer scans the
 * fifodir, keeps only entries whose name starts with "evtsub:" AND whose length
 * is exactly EVENT_FIFO_NAMELEN (39), opens each O_WRONLY|O_NONBLOCK and writes
 * the transition byte. We replicate that filter byte-for-byte: a test that the
 * producer "never sees a visible fifo without a reader" must use this same
 * filter, otherwise it proves nothing about the real producer.
 */
#ifndef EVENT_TEST_HELPERS_H
#define EVENT_TEST_HELPERS_H

#include <stddef.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <66/event.h>

/* Count visible producer-eligible fifos in `dir` (the producer filter). */
static inline int fanout_count(char const *dir)
{
    DIR *d = opendir(dir) ;
    if (!d) return -1 ;
    int n = 0 ;
    struct dirent *e ;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN)) continue ;
        if (strlen(e->d_name) != EVENT_FIFO_NAMELEN) continue ;
        n++ ;
    }
    closedir(d) ;
    return n ;
}

/* Write the `len`-byte frame at `buf` into every visible producer-eligible fifo
 * in `dir`, in ONE write() (a frame is <= EVENT_FRAME_MAX < PIPE_BUF, so the
 * write is atomic like the real producer's). Returns the number of fifos written
 * to in full, or -1 on opendir failure. A fifo with no reader (ENXIO) is skipped
 * (the real producer would too). This replaces the old single-byte fanout: the
 * channel now carries length-framed messages, not bytes. */
static inline int fanout_frame(char const *dir, char const *buf, size_t len)
{
    DIR *d = opendir(dir) ;
    if (!d) return -1 ;
    int n = 0 ;
    char path[1024] ;
    struct dirent *e ;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN)) continue ;
        if (strlen(e->d_name) != EVENT_FIFO_NAMELEN) continue ;
        size_t dl = strlen(dir) ;
        memcpy(path, dir, dl) ;
        path[dl] = '/' ;
        memcpy(path + dl + 1, e->d_name, EVENT_FIFO_NAMELEN + 1) ;
        int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        if (fd < 0) continue ;
        ssize_t w ;
        do w = write(fd, buf, len) ; while (w < 0 && errno == EINTR) ;
        close(fd) ;
        if (w == (ssize_t)len) n++ ;
    }
    closedir(d) ;
    return n ;
}

/* Open EVERY visible producer-eligible fifo O_WRONLY|O_NONBLOCK and return how
 * many opened. `seen_enxio` counts the fifos that were still on disk after the
 * open failed with ENXIO, i.e. genuinely published without a read end.
 *
 * ENXIO alone does not mean that: a producer's open() is not atomic. The name is
 * resolved by the path walk, then fifo_open() takes the pipe mutex and only then
 * tests pipe->readers. A subscriber tearing down in between — unlink first, then
 * close the read end — hands the producer ENXIO on a name that is already gone,
 * which is why the entry is re-checked. In that teardown case the unlink is
 * necessarily complete before the readers test, so a surviving entry can only
 * come from a fifo published, or left, without a reader. */
static inline int fanout_open_ok(char const *dir, int *seen_enxio)
{
    if (seen_enxio) *seen_enxio = 0 ;
    DIR *d = opendir(dir) ;
    if (!d) return -1 ;
    int ok = 0 ;
    char path[1024] ;
    struct dirent *e ;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN)) continue ;
        if (strlen(e->d_name) != EVENT_FIFO_NAMELEN) continue ;
        size_t dl = strlen(dir) ;
        memcpy(path, dir, dl) ;
        path[dl] = '/' ;
        memcpy(path + dl + 1, e->d_name, EVENT_FIFO_NAMELEN + 1) ;
        int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        if (fd < 0) {
            struct stat st ;
            if (errno == ENXIO && seen_enxio && !stat(path, &st))
                (*seen_enxio)++ ;
            continue ;
        }
        ok++ ;
        close(fd) ;
    }
    closedir(d) ;
    return ok ;
}

/* Count all directory entries (any name) — used to assert no inode leaks. */
static inline int dir_entries(char const *dir)
{
    DIR *d = opendir(dir) ;
    if (!d) return -1 ;
    int n = 0 ;
    struct dirent *e ;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
        n++ ;
    }
    closedir(d) ;
    return n ;
}

#endif
