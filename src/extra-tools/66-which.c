/* 
 * 66-which.c
 * 
 * Copyright (c) 2019 Dyne.org Foundation, Amsterdam
 * 
 * Written by:
 *  - Danilo Spinella <danyspin97@protonmail.com>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * */

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <skalibs/genalloc.h>
#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/config.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>

#define USAGE "66-which [ -h ] [ -a ] [ -q ] commands..."

static inline void info_help (void)
{
  static char const *help =
"66-which <options> commands...\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-a: print all matching executable in PATH\n"
"	-q: quiet, do not print anything to stdout\n"
;
	if (buffer_putsflush(buffer_1, help) < 0)
		strerr_diefu1sys(111, "write to stdout") ;
}

int check_executable(char const* filepath) {
	struct stat sb ;
	return (stat(filepath, &sb) == 0 && sb.st_mode & S_IXUSR
			&& !S_ISDIR(sb.st_mode)) ? 1 : 0 ;
}
int parse_path(genalloc_ref folders, char* path) {
	char* rp = NULL ;
	size_t i, len, s ;
	int found ;
	stralloc filepath = STRALLOC_ZERO ;
	while (path) {
		s = str_chr(path, ':') ;
		if (stralloc_copyb(&filepath, path, s) < 0
			|| !stralloc_0(&filepath)) retstralloc(111, "PATH") ;
		rp = realpath(filepath.s, NULL);
		if (rp != NULL) {
			char const** ss = genalloc_s(char const*, folders);
			found = 0;
			len = genalloc_len(char const*, folders);
			for ( i = 0 ; i < len ; i++) {
				if (obstr_equal(ss[i], rp)) {
					found = 1 ;
					break ;
				}
			}
			if (!found) {
				genalloc_append(char const*, folders, &rp) ;
			} else {
				free(rp);
			}
		}
		if (s == strlen(path)) break ;
		path += s + 1;
	}
	stralloc_free(&filepath) ;
	return genalloc_len(char const*, folders) ;
}

int split_filepath(char const* name, stralloc_ref filepath,
					stralloc_ref basename) {
	size_t strip_chars = 1 ;

	switch (name[0]) {
		case '~':
		case '.':
			if (name[0] == '.' && name[1] == '.') {
				if (name[2] != '/') break ;
				strip_chars = 3 ;
			} else if (name[1] != '/') break ;
			if (name[1] == '/') strip_chars = 2 ;
		case '/': 
			if (stralloc_catb(filepath, name, strip_chars) < 0)
				retstralloc(111, "filepath") ;
			name += strip_chars ;
			int d ;
			while ((d = str_chr(name, '/')) != strlen(name)) {
				if (stralloc_catb(filepath, name, d + 1) < 0)
					retstralloc(111, "filepath") ;
				name += d + 1;
			}
			if (!stralloc_0(filepath)
				|| stralloc_copys(basename, name) < 0
				|| !stralloc_0(basename)) retstralloc(111, "filepath") ;
			return 1;
		default: break ;
	}

	return 0;
}

int handle_string(char const* name, char const* env_path, genalloc_ref paths,
				  int quiet, int printall) {
	size_t i, len ;
	int found = 0, string_is_path = 0;

	stralloc filepath = STRALLOC_ZERO ;
	stralloc basename = STRALLOC_ZERO ;
	if (split_filepath(name, &filepath, &basename)) {
		len = 0 ;
		string_is_path = 1 ;
		name = basename.s ;
	}

	char const** ss = genalloc_s(char const*, paths) ;
	len = genalloc_len(char const*, paths) ;

	for (i = 0 ; i < len ; i++) {
		if (!stralloc_copys(&filepath, ss[i])
			|| !stralloc_cats(&filepath, "/")
			|| !stralloc_cats(&filepath, name)
			|| !stralloc_0(&filepath))
			retstralloc(111, "filepath");

		if (check_executable(filepath.s)) {
			if (!quiet && (buffer_puts(buffer_1small, filepath.s) < 0
				|| buffer_put(buffer_1small, "\n", 1) < 0
				|| !buffer_flush(buffer_1small)))
				strerr_diefu1sys(111, "write to stdout") ;
			found = 1;
			if (!printall) break ;
		}
	}

	if (found == 0 && !printall) {
		if (!quiet && (buffer_puts(buffer_2, PROG) < 0
			|| buffer_puts(buffer_2, ": error: no ") < 0
			|| buffer_puts(buffer_2, name) < 0
			|| buffer_puts(buffer_2, " in (") < 0
			|| buffer_puts(buffer_2, string_is_path ? filepath.s : env_path) < 0
			|| buffer_puts(buffer_2, ")\n") < 0)
			|| !buffer_flush(buffer_2))
			strerr_diefu1sys(111, "write to stderr");
	}

	stralloc_free(&filepath);
	stralloc_free(&basename);
	return found == 1 ? 0 : 111;
}

int handle_path(char const* path, int quiet, int printall) {
	char* rp = realpath(path, NULL) ;
	if (rp != NULL && check_executable(rp)) {
		if (!quiet && (buffer_puts(buffer_1small, rp) < 0
			|| buffer_put(buffer_1small, "\n", 1) < 0
			|| !buffer_flush(buffer_1small)))
			strerr_diefu1sys(111, "write to stdout") ;
		return 0 ;
	}

	stralloc filepath = STRALLOC_ZERO ;
	stralloc basename = STRALLOC_ZERO;
	split_filepath(path, &filepath, &basename) ;

	if (!quiet && (buffer_puts(buffer_2, PROG) < 0
		|| buffer_puts(buffer_2, ": error: no ") < 0
		|| buffer_puts(buffer_2, basename.s) < 0
		|| buffer_puts(buffer_2, " in (") < 0
		|| buffer_puts(buffer_2, filepath.s) < 0
		|| buffer_puts(buffer_2, ")\n") < 0
		|| !buffer_flush(buffer_2)))
		strerr_diefu1sys(111, "write to stderr");

	stralloc_free(&filepath) ;
	stralloc_free(&basename) ;
	return 111 ;
}

int main (int argc, char const *const *argv)
{
	char printall = 0 ;
	char quiet = 0 ;
	char* path ;
	char* rp ;
	int ret = 0;
	genalloc paths = GENALLOC_ZERO ;
	PROG = "66-which" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "haq", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h': info_help() ; return 0 ;
				case 'a': printall = 1 ; break ;
				case 'q': quiet = 1 ; break ;
				default : strerr_dieusage(100, USAGE) ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	path = getenv("PATH");
	if (path == NULL) {
		path = SKALIBS_DEFAULTPATH;
	}
	if (! *argv) strerr_dieusage(100, USAGE) ;
	if (parse_path(&paths, path) == 0)
		strerr_diefu1sys(110, "PATH is empty or contains non valid values") ;

	for ( ; *argv ; argv++) {
		if ((*argv)[0] != '/'
			&& ((*argv)[0] != '~' || ((*argv)[0] == '~' && (*argv)[1] != '/'))
			&& ((*argv)[0] != '.' || ((*argv)[0] == '.' && (*argv)[1] != '/')
				|| ((*argv)[0] == '.' && (*argv)[1] == '.' && (*argv)[2] != '/')))
			ret = handle_string(*argv, path, &paths, quiet, printall);
		else
			ret = handle_path(*argv, quiet, printall);
	}

	//free(path) ;
	size_t len = genalloc_len(char const*, &paths) ;
	char** s = genalloc_s(char*, &paths) ;
	for (int i = 0 ; i < len ; i++) {
		free(s[i]) ;
	}
	return ret ;
}
