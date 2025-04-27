CC = gcc
CFLAGS=-g -Wall -Wextra -std=c99 -D_XOPEN_SOURCE=700 -I../../../include

LIBS_PATH = -L/usr/lib/skalibs \
			-L/usr/lib/execline \
            -L/usr/lib/oblibs

LIBS = 	-loblibs \
		-lexecline \
		-lskarnet \
		../../../../lib66.so.xyzzy

LDFLAGS = $(LIBS_PATH) $(LIBS)
# List all source files for tests
# parse_bracket.c another_test.c some_other_test.c
TEST_SOURCES = execl-envfile.c
TEST_BINARIES = $(TEST_SOURCES:.c=)

all: $(TEST_BINARIES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TEST_BINARIES)
	@for binary in $(TEST_BINARIES); do \
		echo "Executing $$binary"; \
		LD_PRELOAD=$(abspath ../../../../lib66.so.xyzzy) LD_LIBRARY_PATH=../../../../:$$LD_LIBRARY_PATH ./$$binary || exit 1; \
	done

clean:
	@exec rm -f $(TEST_BINARIES) $(wildcard *.o *.lo)

.PHONY: run clean