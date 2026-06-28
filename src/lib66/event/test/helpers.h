/* helpers.h — test scaffolding shared by the event test files.
 *
 * fanout_*: mimic 66-supervise's fanout. The real producer scans the
 * fifodir, keeps only entries whose name starts with "ftrig1:" AND whose length
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

/* Write byte `b` into every visible producer-eligible fifo in `dir`.
 * Returns the number of fifos written to, or -1 on opendir failure.
 * A fifo with no reader (ENXIO) is skipped (the real producer would too). */
static inline int fanout_write(char const *dir, char b)
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
        do w = write(fd, &b, 1) ; while (w < 0 && errno == EINTR) ;
        close(fd) ;
        if (w == 1) n++ ;
    }
    closedir(d) ;
    return n ;
}

/* Open EVERY visible producer-eligible fifo O_WRONLY|O_NONBLOCK and return how
 * many opened without ENXIO. Used to prove the trick: a fifo is never published
 * visibly until its read end exists, so a producer opening it never gets ENXIO. */
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
        if (fd < 0) { if (errno == ENXIO && seen_enxio) (*seen_enxio)++ ; continue ; }
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
