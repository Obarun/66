#include <assert.h>
#include <stdio.h>
#include <oblibs/stack.h>
#include <66/parse.h>
#include <66/enum.h>

void basic()
{
    printf("Running test basic...\n") ;

    int r ;
    const char *a = "= test(valid()))\n" ;
    const char *b = "valid())" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void basic1()
{
    printf("Running test basic1...\n") ;

    int r ;
    const char *a = "= test(valid)\n" ;
    const char *b = "valid" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void basic2()
{
    printf("Running test basic2...\n") ;

    int r ;
    const char *a = "= \n(\n\tvalid\n)\n\nype=\n" ;
    const char *b = "\n\tvalid\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void basic3()
{
    printf("Running test basic3...\n") ;

    int r ;
    const char *a = "Execute = ( umask 027 %%BINDIR%%/deluge-web -d )# comment\n" ;
    const char *b = " umask 027 %%BINDIR%%/deluge-web -d " ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void basic4()
{
    printf("Running test basic4...\n") ;

    int r ;
    const char *a = "(\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n\
)\n\
# comment\n\
Bad=key\n" ;

    const char *b = "\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void basic5()
{
    printf("Running test basic5...\n") ;

    int r ;
    const char *a = "(\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n\
)\n\
Type=classic\n" ;

    const char *b = "\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void comment_inside()
{
    printf("Running test comment_inside...\n") ;

    int r ;
    const char *a = "= \n\
(\n\
##comment\n\
    # comment1\n\
valid\n\
)\n\
[Start]\n" ;
    const char *b = "\n\
##comment\n\
    # comment1\n\
valid\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void comment_outside()
{
    printf("Running test comment_outside...\n") ;

    int r ;
    const char *a = "= \n\
(\n\
##comment\n\
    # comment1\n\
valid\n\
)\n\
# lowercase\n\
# Type=classic\n" ;

    const char *b = "\n\
##comment\n\
    # comment1\n\
valid\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void comment_outside_valid()
{
    printf("Running test comment_outside_valid...\n") ;

    int r ;
    const char *a = "= \n\
(\n\
##comment\n\
    # comment1\n\
valid\n\
)\n\
# lowercase\n\
# Type=classic\n\
Description = \"valid\"" ;

    const char *b = "\n\
##comment\n\
    # comment1\n\
valid\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void odd_bracket()
{
    printf("Running test odd_bracket...\n") ;

    int r ;
    const char *a = "= (valid\nTpe=classic\n" ;
    const char *b = "valid" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 0) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void odd_bracket_validkey()
{
    printf("Running test odd_bracket_validkey...\n") ;

    int r ;
    const char *a = "= (valid\nType=classic\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 0) ;
}

void no_bracket()
{
    printf("Running test no_bracket...\n") ;

    int r ;
    const char *a = "= testvalid\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 0) ;
}

void nested_bracket()
{
    printf("Running test nested_bracket...\n") ;

    int r ;
    const char *a = "= ((nested)(brackets))\nType=classic\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void bash()
{
    printf("Running test bash...\n") ;

    int r ;
    const char *a = "=(#!/usr/bin/sh\n\
\n\
exec 2>&1\n\
\n\
if ! [ \"${NVPD_USER}x\" = x ]; then\n\
    66-yeller \"Starting nvidia-persistenced for ${NVPD_USER}\"\n\
    NVPD_USER_ARG=\"--user ${NVPD_USER}\"\n\
else\n\
    66-yeller \"Starting nvidia-persistenced\"\n\
fi\n\
\n\
start-stop-daemon --start --quiet --pidfile ${pidfile}\n\
    --background --exec /usr/bin/nvidia-persistenced\n\
    -- ${NVPD_USER_ARG} ${ARGS}\n\
)\n" ;

    const char *b = "#!/usr/bin/sh\n\
\n\
exec 2>&1\n\
\n\
if ! [ \"${NVPD_USER}x\" = x ]; then\n\
    66-yeller \"Starting nvidia-persistenced for ${NVPD_USER}\"\n\
    NVPD_USER_ARG=\"--user ${NVPD_USER}\"\n\
else\n\
    66-yeller \"Starting nvidia-persistenced\"\n\
fi\n\
\n\
start-stop-daemon --start --quiet --pidfile ${pidfile}\n\
    --background --exec /usr/bin/nvidia-persistenced\n\
    -- ${NVPD_USER_ARG} ${ARGS}\n" ;

    _alloc_stk_(stk, strlen(a) + 1) ;
    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void bash_case()
{
    printf("Running test bash_case...\n") ;

    int r ;
    const char *a = "= (#!/bin/bash\n\
case EXPRESSION in\n\
\n\
  PATTERN_1)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  PATTERN_2)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  PATTERN_N)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  *)\n\
    STATEMENTS\n\
    ;;\n\
esac)\n";

    const char *b = "#!/bin/bash\n\
case EXPRESSION in\n\
\n\
  PATTERN_1)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  PATTERN_2)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  PATTERN_N)\n\
    STATEMENTS\n\
    ;;\n\
\n\
  *)\n\
    STATEMENTS\n\
    ;;\n\
esac" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void invalid_key()
{
    printf("Running test invalid_key...\n") ;

    int r ;
    const char *a = "((nested)(brackets))\nTpe=classic\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void invalid_key1()
{
    printf("Running test invalid_key1...\n") ;

    int r ;
    const char *a = "((nested)(brackets))\nTpe=classic\nDescription = \"test\""; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void eof()
{
    printf("Running test eof...\n") ;

    int r ;
    const char *a = "((nested)(brackets))\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void eof_invalid()
{
    printf("Running test eof_invalid...\n") ;

    int r ;
    const char *a = "Options = (log)\n\
UnknownKey = InvalidValues\n"; ;
    const char *b = "log" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void valid_section()
{
    printf("Running test valid_section...\n") ;

    int r ;
    const char *a = "= ((nested)(brackets))\n\n[Start]\nExecute=\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void invalid_section()
{
    printf("Running test invalid_section...\n") ;

    int r ;
    const char *a = "= ((nested)(brackets))\n\n[start]\nExecute=\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_stk_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, SECTION_MAIN) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
}

int main(void) {

    basic() ;
    basic1() ;
    basic2() ;
    basic3() ;
    basic4() ;
    basic5() ;
    comment_inside() ;
    comment_outside() ;
    comment_outside_valid() ;
    odd_bracket() ;
    odd_bracket_validkey() ;
    no_bracket() ;
    nested_bracket() ;
    bash() ;
    bash_case() ;
    invalid_key() ;
    invalid_key1() ;
    eof() ;
    eof_invalid() ;
    valid_section() ;
    invalid_section() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}