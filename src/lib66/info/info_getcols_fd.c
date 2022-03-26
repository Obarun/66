/*
 * info_getcols_fd.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * */

#include <unistd.h>
#include <sys/ioctl.h>
#include <wchar.h>

#include <66/info.h>

int info_getcols_fd(int fd)
{
    int width = -1;

    if(!isatty(fd)) return 0;

#if defined(TIOCGSIZE)
    struct ttysize win;
    if(ioctl(fd, TIOCGSIZE, &win) == 0)
        width = win.ts_cols;
#elif defined(TIOCGWINSZ)
    struct winsize win;
    if(ioctl(fd, TIOCGWINSZ, &win) == 0)
        width = win.ws_col;
#endif

    // return abitrary value
    if(width <= 0) return 100 ;

    return width;
}
