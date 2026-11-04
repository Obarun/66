#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>

#include <66/parse.h>
#include <66/enum_parser.h>

void basic(resolve_enum_table_t table)
{
    printf("Running test basic...\n") ;

    int r ;
    size_t end ;
    const char *a = "= test(valid()))\n" ;
    const char *b = "valid())" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void basic1(resolve_enum_table_t table)
{
    printf("Running test basic1...\n") ;

    int r ;
    size_t end ;
    const char *a = "= test(valid)\n" ;
    const char *b = "valid" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void basic2(resolve_enum_table_t table)
{
    printf("Running test basic2...\n") ;

    int r ;
    size_t end ;
    const char *a = "= \n(\n\tvalid\n)\n\nype=\n" ;
    const char *b = "\n\tvalid\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void basic3(resolve_enum_table_t table)
{
    printf("Running test basic3...\n") ;

    int r ;
    size_t end ;
    const char *a = "Execute = ( umask 027 %%BINDIR%%/deluge-web -d )# comment\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 0) ;
}

void basic4(resolve_enum_table_t table)
{
    printf("Running test basic4...\n") ;

    int r ;
    size_t end ;
    const char *a = "(\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n\
)\n\
# comment\n\
Bad=key\n" ;

    const char *b = "\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void basic5(resolve_enum_table_t table)
{
    printf("Running test basic5...\n") ;

    int r ;
    size_t end ;
    const char *a = "(\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n\
)\n\
Type=classic\n" ;

    const char *b = "\n\
    execl-toc -d /run/fail2ban\n\
    %%BINDIR%%/fail2ban-server -xf --logtarget=SYSOUT start\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void comment_inside(resolve_enum_table_t table)
{
    printf("Running test comment_inside...\n") ;

    int r ;
    size_t end ;
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

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void comment_outside(resolve_enum_table_t table)
{
    printf("Running test comment_outside...\n") ;

    int r ;
    size_t end ;
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

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void comment_outside_valid(resolve_enum_table_t table)
{
    printf("Running test comment_outside_valid...\n") ;

    int r ;
    size_t end ;
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

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void odd_bracket(resolve_enum_table_t table)
{
    printf("Running test odd_bracket...\n") ;

    int r ;
    size_t end ;
    const char *a = "= (valid\nTpe=classic\n" ;
    const char *b = "valid" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 0) ;
    assert(strcmp(stk.s, b) == 0) ;
}

void odd_bracket_validkey(resolve_enum_table_t table)
{
    printf("Running test odd_bracket_validkey...\n") ;

    int r ;
    size_t end ;
    const char *a = "= (valid\nType=classic\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 0) ;
}

void no_bracket(resolve_enum_table_t table)
{
    printf("Running test no_bracket...\n") ;

    int r ;
    size_t end ;
    const char *a = "= testvalid\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 0) ;
}

void nested_bracket(resolve_enum_table_t table)
{
    printf("Running test nested_bracket...\n") ;

    int r ;
    size_t end ;
    const char *a = "= ((nested)(brackets))\nType=classic\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void bash(resolve_enum_table_t table)
{
    printf("Running test bash...\n") ;

    int r ;
    size_t end ;
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

    _alloc_sbl_(stk, strlen(a) + 1) ;
    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void bash_case(resolve_enum_table_t table)
{
    printf("Running test bash_case...\n") ;

    int r ;
    size_t end ;
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

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void invalid_key(resolve_enum_table_t table)
{
    printf("Running test invalid_key...\n") ;

    int r ;
    size_t end ;
    const char *a = "((nested)(brackets))\nTpe=classic\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void invalid_key1(resolve_enum_table_t table)
{
    printf("Running test invalid_key1...\n") ;

    int r ;
    size_t end ;
    const char *a = "((nested)(brackets))\nTpe=classic\nDescription = \"test\""; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void eof(resolve_enum_table_t table)
{
    printf("Running test eof...\n") ;

    int r ;
    size_t end ;
    const char *a = "((nested)(brackets))\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void eof_invalid(resolve_enum_table_t table)
{
    printf("Running test eof_invalid...\n") ;

    int r ;
    size_t end ;
    const char *a = "Options = (log)\n\
UnknownKey = InvalidValues\n"; ;
    const char *b = "log" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void valid_section(resolve_enum_table_t table)
{
    printf("Running test valid_section...\n") ;

    int r ;
    size_t end ;
    const char *a = "= ((nested)(brackets))\n\n[Start]\nExecute=\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void invalid_section(resolve_enum_table_t table)
{
    printf("Running test invalid_section...\n") ;

    int r ;
    size_t end ;
    const char *a = "= ((nested)(brackets))\n\n[start]\nExecute=\n"; ;
    const char *b = "(nested)(brackets)" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 1) ;
    assert(strcmp(stk.s, b) == 0) ;
    assert(a[end] == ')') ;
}

void empty_bracket(resolve_enum_table_t table)
{
    printf("Running test empty_bracket...\n") ;

    int r ;
    size_t end ;
    const char *a = "= ()\n" ;

    _alloc_sbl_(stk, strlen(a) + 1) ;

    // nothing between the brackets: a fault of the frontend, not of the system
    r = parse_bracket(&stk, a, &end, table) ;
    assert(r == 0) ;
}

/**
 * Whole frontends through the first pass of the parse, parse_store_build():
 * sections, keys and values, the path where a key is told unknown. Each case
 * asserts the return, the exact text written to stderr at verbosity 2, and
 * the stored value of each key it names, or its absence.
 */

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

// the first pass refuses a frontend whose first section is not [Main]
#define MAIN_HEAD "[Main]\nType = classic\n"

#define WARN_FORMAT(key, section) "parse: warning: invalid format of key: " key ": in section: " section "\n"

#define WARN_UNKNOWN(key) "parse: warning: unknown key: " key " -- ignoring it\n"

typedef struct value_s value_t ;
struct value_s
{
    uint32_t sid ;
    uint32_t kid ;
    char const *value ; // 0: the key is absent
} ;

typedef struct frontend_case_s frontend_case_t ;
struct frontend_case_s
{
    char const *name ;
    char const *frontend ;
    int ret ;
    char const *said ; // the exact output of the parse at verbosity 2
    value_t values[8] ;
    unsigned int nvalues ;
} ;

// parse @fe at verbosity 2 with stderr written to a file, read back into @said
static int build_said(parse_store_t *st, char const *fe, char *said, size_t len)
{
    unsigned int verbosity = VERBOSITY ;
    FILE *t = tmpfile() ;
    CHECK(t, "tmpfile") ;

    fflush(stderr) ;
    int saved = dup(2) ;
    CHECK(saved >= 0 && dup2(fileno(t), 2) == 2, "redirect stderr") ;

    VERBOSITY = 2 ;
    int r = parse_store_build(st, fe) ;
    VERBOSITY = verbosity ;

    fflush(stderr) ;
    CHECK(dup2(saved, 2) == 2, "restore stderr") ;
    close(saved) ;

    rewind(t) ;
    size_t n = fread(said, 1, len - 1, t) ;
    said[n] = 0 ;
    fclose(t) ;

    return r ;
}

static void run_case(frontend_case_t const *c)
{
    printf("Running frontend case %s...\n", c->name) ;

    char said[4096] ;
    parse_store_t st ;

    int r = build_said(&st, c->frontend, said, sizeof(said)) ;

    CHECK(r == c->ret, "%s: parse_store_build returned %d, expected %d, the parse said:\n%s", c->name, r, c->ret, said) ;
    CHECK(!strcmp(said, c->said), "%s: the parse said:\n%s\nexpected:\n%s", c->name, said, c->said) ;

    for (unsigned int i = 0 ; i < c->nvalues ; i++) {

        value_t const *v = &c->values[i] ;
        size_t len = 0 ;
        char const *got = parse_store_get(&st, v->sid, v->kid, &len) ;

        CHECK(parse_store_present(&st, v->sid, v->kid) == (v->value ? 1 : 0), "%s: value %u present is %u", c->name, i, parse_store_present(&st, v->sid, v->kid)) ;

        if (!v->value)
            CHECK(!got, "%s: value %u is present: [%s], expected absent", c->name, i, got) ;
        else
            CHECK(got && !strcmp(got, v->value), "%s: value %u is [%s], expected [%s]", c->name, i, got ? got : "(absent)", v->value) ;
    }

    parse_store_free(&st) ;
}

#define S_MAIN E_PARSER_SECTION_MAIN
#define S_START E_PARSER_SECTION_START
#define S_STOP E_PARSER_SECTION_STOP
#define S_LOGGER E_PARSER_SECTION_LOGGER
#define S_ENV E_PARSER_SECTION_ENVIRONMENT
#define S_REGEX E_PARSER_SECTION_REGEX
#define S_EXEC E_PARSER_SECTION_EXECUTE
#define S_EVENT E_PARSER_SECTION_EVENT

static frontend_case_t const frontend_cases[] = {

    // a word of a value is never a key
    { "capital_in_value", MAIN_HEAD "[Start]\nTimeout = 3000\nExecute = ( echo $Toto )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3000" }, { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " echo $Toto " } }, 2 },
    { "capital_in_value_then_key", MAIN_HEAD "[Start]\nExecute = ( echo $Toto )\nTimeout = 3000\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " echo $Toto " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3000" } }, 2 },
    { "mktemp_template", MAIN_HEAD "[Start]\nExecute = ( foreground { mktemp /run/c-crash.runs.XXXXXX } false )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " foreground { mktemp /run/c-crash.runs.XXXXXX } false " } }, 1 },
    { "braced_variable", MAIN_HEAD "[Start]\nExecute = ( /usr/bin/66-userd -d3 -v${Verbosity} )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " /usr/bin/66-userd -d3 -v${Verbosity} " } }, 1 },
    { "equal_in_value", MAIN_HEAD "[Start]\nExecute = ( A=B foo )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " A=B foo " } }, 1 },
    { "multiline_script", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\nFOO=Bar\necho \"$FOO\" in Here\nexec Sleep 1\n)\nTimeout = 7\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\nFOO=Bar\necho \"$FOO\" in Here\nexec Sleep 1\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "7" } }, 2 },
    { "nested_parentheses", MAIN_HEAD "[Start]\nExecute = ( if { test (A) } Foo (B) )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " if { test (A) } Foo (B) " } }, 1 },
    { "shell_case", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\ncase $X in\n  A) echo One ;;\n  *) echo Two ;;\nesac\n)\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\ncase $X in\n  A) echo One ;;\n  *) echo Two ;;\nesac\n" } }, 1 },

    // a line of a value that names a key of its section is neither stored nor refused
    { "key_name_inside_value", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\nTimeoutStart=7\nTimeout=9\n)\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\nTimeoutStart=7\nTimeout=9\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMESTART, 0 }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, 0 } }, 3 },
    { "key_name_inside_value_inline", MAIN_HEAD "[Start]\nExecute = ( echo TimeoutStart=7 )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " echo TimeoutStart=7 " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMESTART, 0 } }, 2 },
    { "own_key_inside_value", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\nExecute=1\n)\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\nExecute=1\n" } }, 1 },

    // an unknown key is told, once
    { "unknown_key_after_value", MAIN_HEAD "[Start]\nExecute = ( echo $Toto )\nMykey = make it crashing\n", 1, WARN_UNKNOWN("Mykey"),
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " echo $Toto " } }, 1 },
    { "unknown_key_alone", "[Main]\nMykey = x\n", 1, WARN_UNKNOWN("Mykey"), { { 0 } }, 0 },
    { "unknown_key_between", "[Main]\nType = classic\nBadKey = 1\nVersion = 0.0.1\n", 1, WARN_UNKNOWN("BadKey"),
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" }, { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1" } }, 2 },
    { "unknown_key_first_line", "[Main]\nFoo = 1\nType = classic\n", 1, WARN_UNKNOWN("Foo"),
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 1 },
    { "unknown_key_capital_value", "[Main]\nFoo = Bar Baz Description\nType = classic\n", 1, WARN_UNKNOWN("Foo"),
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" }, { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, 0 } }, 2 },
    { "two_unknown_keys", "[Main]\nFoo = 1\nBar = 2\n", 1, WARN_UNKNOWN("Foo") WARN_UNKNOWN("Bar"), { { 0 } }, 0 },

    // a line which begins with '#', blanks aside, is a comment
    { "comment_words", "[Main]\n# Some Comment Here\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 1 },
    { "commented_unknown_key", "[Main]\n#Mykey = x\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 1 },
    { "commented_known_key", "[Main]\n#Description = \"Old Value\"\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, 0 }, { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 2 },
    { "indented_comment", "[Main]\nType = classic\n   # Indented Note\n\t#Version = 9\nVersion = 0.0.1\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" }, { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1" } }, 2 },
    { "comment_first_line", "[Main]\n# First Line\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 1 },
    // a '#' on the line of a ')' is refused, glued to it or not
    { "comment_after_bracket", MAIN_HEAD "[Start]\nExecute = ( true ) # note\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "hash_after_bracket", MAIN_HEAD "[Start]\nExecute = ( true )#Timeout = 3333\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "hash_comment_after_bracket", MAIN_HEAD "[Start]\nExecute = ( true )# note\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "hash_alone_after_bracket", MAIN_HEAD "[Start]\nExecute = ( true )#\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "key_glued_to_bracket", MAIN_HEAD "[Start]\nExecute = ( true )Timeout = 3333\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "hash_after_glued_brackets", MAIN_HEAD "[Start]\nExecute = ( (a))#Timeout = 3333\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "section_glued_to_bracket", MAIN_HEAD "[Start]\nExecute = ( true )[Stop]\nExecute = ( echo s )\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "glued_brackets_then_key", MAIN_HEAD "[Start]\nExecute = ( echo $((1+2)))\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " echo $((1+2))" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },

    // the line of the closing ')' of a value on several lines holds nothing else
    { "key_after_multiline_bracket", MAIN_HEAD "[Start]\nExecute = (\ntrue\n)#Timeout = 3333\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "key_glued_to_multiline_bracket", MAIN_HEAD "[Start]\nExecute = (\ntrue\n)Timeout = 3333\nTimeout = 7\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "comment_after_multiline_bracket", MAIN_HEAD "[Start]\nExecute = (\ntrue\n) # note\nTimeout = 7\n", 0, WARN_FORMAT("Execute", "Start"), { { 0 } }, 0 },
    { "blanks_after_multiline_bracket", MAIN_HEAD "[Start]\nExecute = (\ntrue\n) \t\nTimeout = 7\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\ntrue\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "7" } }, 2 },
    { "script_parenthese_with_text", MAIN_HEAD "[Start]\nExecute = (\necho a\n) foo\n)\nTimeout = 7\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\necho a\n) foo\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "7" } }, 2 },

    // a key counts only where it begins its line, blanks aside
    { "capital_after_quote", "[Main]\nType = classic\nDescription = \"x\" # Note\nVersion = 0.0.1\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "x" }, { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1" } }, 2 },
    { "key_after_quote", "[Main]\nType = classic\nDescription = \"x\" Version = 9\nVersion = 0.0.1\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "x" }, { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1" } }, 2 },

    // where a value between parentheses ends: the line after the ')' is a key, a comment or a section
    { "bracket_then_commented_key", MAIN_HEAD "[Start]\nExecute = ( true )\n#Timeout = 5\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "bracket_then_commented_unknown_key", MAIN_HEAD "[Start]\nExecute = ( true )\n#Bogus = 5\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "bracket_blank_then_commented_unknown_key", MAIN_HEAD "[Start]\nExecute = ( true )\n\n#Bogus = 5\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },

    // a value of the wrong shape is refused
    { "quote_without_quotes", "[Main]\nDescription = no quotes\nType = classic\n", 0, WARN_FORMAT("Description", "Main"), { { 0 } }, 0 },
    { "quote_not_closed", "[Main]\nDescription = \"no end\nType = classic\n", 0, WARN_FORMAT("Description", "Main"), { { 0 } }, 0 },
    { "quote_on_next_line", "[Main]\nDescription =\n\"next line\"\nType = classic\n", 0, WARN_FORMAT("Description", "Main"), { { 0 } }, 0 },
    { "quote_after_text", "[Main]\nDescription = some \"text\"\nType = classic\n", 0, WARN_FORMAT("Description", "Main"), { { 0 } }, 0 },
    { "quote_in_regex", "[Main]\nType = module\n[Regex]\nConfigure = --flag\n", 0, WARN_FORMAT("Configure", "Regex"), { { 0 } }, 0 },
    { "bracket_without_parentheses", "[Main]\nDepends = foo\n", 0, WARN_FORMAT("Depends", "Main"), { { 0 } }, 0 },
    // an empty value is stored empty: what it means is the second pass's, which refuses an empty Type
    { "line_without_value", "[Main]\nType =\nVersion = 0.0.1\n", 1, "", { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "" }, { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1" } }, 2 },

    // the sections of a frontend
    { "section_name_inside_quote", "[Main]\nType = classic\nDescription = \"no [Environment] section\"\nOptions = ( !log )\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "no [Environment] section" }, { S_MAIN, E_PARSER_SECTION_MAIN_OPTIONS, " !log " }, { S_ENV, E_PARSER_SECTION_ENVIRON_ENVAL, 0 } }, 3 },
    { "script_test_bracket", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\n[ -f /etc/Foo ] && exit 0\nexit 1\n)\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\n[ -f /etc/Foo ] && exit 0\nexit 1\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "script_open_square_bracket", MAIN_HEAD "[Start]\nExecute = (\nfoo )\n[ bar\n)\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\nfoo )\n[ bar\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "script_square_word", MAIN_HEAD "[Start]\nExecute = (\nfoo )\n[NotAName]\n)\nTimeout = 3\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\nfoo )\n[NotAName]\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "commented_section", MAIN_HEAD "#[Stop]\n[Start]\nExecute = ( true )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_STOP, E_PARSER_SECTION_STARTSTOP_EXEC, 0 } }, 2 },
    { "indented_section", MAIN_HEAD " \t[Start]\nExecute = ( true )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 1 },
    { "section_trailing_blanks", MAIN_HEAD "[Start] \t\nExecute = ( true )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 1 },
    { "unknown_section_name", MAIN_HEAD "[Strat]\n", 1, "parse: warning: invalid section name: Strat -- ignoring it\n",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 1 },
    { "section_trailing_text", MAIN_HEAD "[Start] text # note\nExecute = ( true )\n", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 1 },
    { "section_lowercase", MAIN_HEAD "[start]\nExecute = ( true )\n", 1, WARN_UNKNOWN("Execute"),
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, 0 } }, 1 },
    // a line value drops its blanks (LEXER_CONFIG_INLINE skips them): the [Stop] stays in it, no section opens
    { "section_mid_line", "[Main]\nType = classic\nVersion = 0.0.1 [Stop]\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_VERSION, "0.0.1[Stop]" } }, 1 },
    { "section_trailing_comment", "[Main] # the identity\nType = classic\n[Start]\t# the start\nExecute = ( true )\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" }, { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 2 },

    // an unknown header ends the section before it: the lines below it belong to no section
    { "unknown_section_keeps_previous", MAIN_HEAD "[Start]\nExecute = ( true )\nTimeout = 3\n[Stp]\nExecute = ( echo stop )\nTimeout = 9\n", 1, "parse: warning: invalid section name: Stp -- ignoring it\n",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "unknown_section_with_comment", MAIN_HEAD "[Start]\nExecute = ( true )\n[Stp] # a typo\nExecute = ( echo stop )\n", 1, "parse: warning: invalid section name: Stp -- ignoring it\n",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 1 },
    { "unknown_section_then_known", MAIN_HEAD "[Start]\nExecute = ( true )\n[Stp]\nExecute = ( echo stop )\n[Stop]\nExecute = ( echo bye )\n", 1, "parse: warning: invalid section name: Stp -- ignoring it\n",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " }, { S_STOP, E_PARSER_SECTION_STARTSTOP_EXEC, " echo bye " } }, 2 },
    // a header line inside a value is a line of the value: only the section reader, which reads no value, tells it
    { "script_writes_ini", MAIN_HEAD "[Start]\nExecute = (\n#!/bin/sh\ncat > /run/foo.ini <<EOF\n[Unit]\nName=Foo\nEOF\n)\nTimeout = 3\n", 1, "parse: warning: invalid section name: Unit -- ignoring it\n",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, "\n#!/bin/sh\ncat > /run/foo.ini <<EOF\n[Unit]\nName=Foo\nEOF\n" }, { S_START, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "3" } }, 2 },
    { "section_last_line_without_newline", MAIN_HEAD "[Start]\nExecute = ( true )\n[Stop]", 1, "",
      { { S_START, E_PARSER_SECTION_STARTSTOP_EXEC, " true " } }, 1 },
    { "no_section", "Type = classic\n", 0, "parse: warning: invalid frontend file -- no section found\n", { { 0 } }, 0 },
    { "main_not_first", "[Start]\nExecute = ( true )\n[Main]\nType = classic\n", 0, "parse: warning: invalid frontend file -- section [Main] must be set first\n", { { 0 } }, 0 },
    { "too_many_sections", "[Main]\n[Start]\n[Stop]\n[Logger]\n[Environment]\n[Regex]\n[Execute]\n[Event]\n[Start]\n", 0, "parse: warning: too many sections in frontend file\n", { { 0 } }, 0 },

    // a value between double quotes, on the line of its key
    { "quote_glued", "[Main]\nDescription=\"Glued Value\"\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "Glued Value" }, { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 2 },
    { "quote_blanks", "[Main]\nDescription = \t \"Blank Value\"\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "Blank Value" }, { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 2 },
    { "quote_empty", "[Main]\nDescription = \"\"\nType = classic\n", 1, "",
      { { S_MAIN, E_PARSER_SECTION_MAIN_DESCRIPTION, "" }, { S_MAIN, E_PARSER_SECTION_MAIN_TYPE, "classic" } }, 2 },

    // the other sections
    // the body is kept raw, its header line aside, with the newline parse_store_build() closes every body with
    { "environment", MAIN_HEAD "[Environment]\n# A Comment\nFOO=Bar\nPATH=/usr/Bin\n", 1, "",
      { { S_ENV, E_PARSER_SECTION_ENVIRON_ENVAL, "# A Comment\nFOO=Bar\nPATH=/usr/Bin\n\n" } }, 1 },
    { "regex", MAIN_HEAD "[Regex]\nConfigure = \"--Flag\"\nInFiles = ( ::User=@I Foo=Bar )\n", 1, "",
      { { S_REGEX, E_PARSER_SECTION_REGEX_CONFIGURE, "--Flag" }, { S_REGEX, E_PARSER_SECTION_REGEX_INFILES, " ::User=@I Foo=Bar " } }, 2 },
    { "event", MAIN_HEAD "[Event]\nEventType = service\nFrom = ( Foo Bar )\nOn = ( up )\nDo = start\n", 1, "",
      { { S_EVENT, E_PARSER_SECTION_EVENT_FROM, " Foo Bar " }, { S_EVENT, E_PARSER_SECTION_EVENT_ON, " up " }, { S_EVENT, E_PARSER_SECTION_EVENT_DO, "start" } }, 3 },
    { "logger", MAIN_HEAD "[Logger]\nExecute = ( Foo $Bar )\nBackup = 3\n", 1, "",
      { { S_LOGGER, E_PARSER_SECTION_LOGGER_EXEC, " Foo $Bar " }, { S_LOGGER, E_PARSER_SECTION_LOGGER_BACKUP, "3" } }, 2 },
    { "execute", MAIN_HEAD "[Execute]\nCapsBound = ( CAP_NET_ADMIN Foo )\nLimitNOFILE = 1024\n", 1, "",
      { { S_EXEC, E_PARSER_SECTION_EXECUTE_CAPS_BOUND, " CAP_NET_ADMIN Foo " }, { S_EXEC, E_PARSER_SECTION_EXECUTE_LIMITNOFILE, "1024" } }, 2 },
    { "stop", MAIN_HEAD "[Stop]\nExecute = ( echo Stopping $Name )\nTimeout = 5\n", 1, "",
      { { S_STOP, E_PARSER_SECTION_STARTSTOP_EXEC, " echo Stopping $Name " }, { S_STOP, E_PARSER_SECTION_STARTSTOP_TIMEOUT, "5" } }, 2 },
} ;

static void frontend_cases_run(void)
{
    for (size_t i = 0 ; i < sizeof(frontend_cases) / sizeof(frontend_cases[0]) ; i++)
        run_case(&frontend_cases[i]) ;
}

// a section or a key beyond the tables reads absent, never out of bounds
static void store_out_of_bounds(void)
{
    printf("Running store out of bounds...\n") ;

    char said[4096] ;
    parse_store_t st ;

    CHECK(build_said(&st, "[Main]\nType = classic\n", said, sizeof(said)) == 1, "parse_store_build: %s", said) ;
    CHECK(!parse_store_present(&st, E_PARSER_SECTION_ENDOFKEY, 0), "a section beyond the table reads present") ;
    CHECK(!parse_store_present(&st, E_PARSER_SECTION_MAIN, SS_PARSE_STORE_NKEY), "a key beyond the table reads present") ;
    CHECK(!parse_store_get(&st, E_PARSER_SECTION_ENDOFKEY, 0, 0), "a section beyond the table reads a value") ;
    CHECK(!parse_store_get(&st, E_PARSER_SECTION_MAIN, SS_PARSE_STORE_NKEY, 0), "a key beyond the table reads a value") ;

    parse_store_free(&st) ;
}

typedef struct section_keys_s section_keys_t ;
struct section_keys_s
{
    uint32_t sid ;
    char const *header ;
    key_description_t const *list ;
    uint32_t nkeys ;
} ;

/**
 * Every key of every section, written once with a valid value whose words
 * carry capitals: each is stored exactly, none is told unknown. A bracket
 * value is followed by the next key, so the reader resumes after it.
 */
static void every_key_of_every_section(void)
{
    static section_keys_t const sections[] = {
        { S_MAIN, "[Main]", enum_list_parser_section_main, E_PARSER_SECTION_MAIN_ENDOFKEY },
        { S_START, "[Start]", enum_list_parser_section_startstop, E_PARSER_SECTION_STARTSTOP_ENDOFKEY },
        { S_STOP, "[Stop]", enum_list_parser_section_startstop, E_PARSER_SECTION_STARTSTOP_ENDOFKEY },
        { S_LOGGER, "[Logger]", enum_list_parser_section_logger, E_PARSER_SECTION_LOGGER_ENDOFKEY },
        { S_REGEX, "[Regex]", enum_list_parser_section_regex, E_PARSER_SECTION_REGEX_ENDOFKEY },
        { S_EXEC, "[Execute]", enum_list_parser_section_execute, E_PARSER_SECTION_EXECUTE_ENDOFKEY },
        { S_EVENT, "[Event]", enum_list_parser_section_event, E_PARSER_SECTION_EVENT_ENDOFKEY },
    } ;

    for (size_t s = 0 ; s < sizeof(sections) / sizeof(sections[0]) ; s++) {

        section_keys_t const *sec = &sections[s] ;
        char fe[8192] ;
        size_t len = 0 ;

        printf("Running every key of section %s...\n", sec->header) ;

        if (sec->sid != S_MAIN)
            len += (size_t)snprintf(fe + len, sizeof(fe) - len, "%s", MAIN_HEAD) ;

        len += (size_t)snprintf(fe + len, sizeof(fe) - len, "%s\n", sec->header) ;

        for (uint32_t k = 0 ; k < sec->nkeys ; k++) {

            char const *name = *sec->list[k].name ;

            switch (sec->list[k].expected) {
                case E_PARSER_EXPECT_QUOTE :
                    len += (size_t)snprintf(fe + len, sizeof(fe) - len, "%s = \"Quoted %s Value\"\n", name, name) ;
                    break ;
                case E_PARSER_EXPECT_BRACKET :
                    len += (size_t)snprintf(fe + len, sizeof(fe) - len, "%s = ( Bracket $%s Value )\n", name, name) ;
                    break ;
                default :
                    len += (size_t)snprintf(fe + len, sizeof(fe) - len, "%s = Line%s\n", name, name) ;
                    break ;
            }
            CHECK(len < sizeof(fe), "%s: frontend too large", sec->header) ;
        }

        char said[4096] ;
        parse_store_t st ;
        int r = build_said(&st, fe, said, sizeof(said)) ;

        CHECK(r == 1, "%s: parse_store_build returned %d, the parse said:\n%s", sec->header, r, said) ;
        CHECK(!said[0], "%s: the parse said:\n%s", sec->header, said) ;

        for (uint32_t k = 0 ; k < sec->nkeys ; k++) {

            char const *name = *sec->list[k].name ;
            char expected[256] ;
            size_t vlen = 0 ;
            char const *got = parse_store_get(&st, sec->sid, k, &vlen) ;

            switch (sec->list[k].expected) {
                case E_PARSER_EXPECT_QUOTE :
                    snprintf(expected, sizeof(expected), "Quoted %s Value", name) ;
                    break ;
                case E_PARSER_EXPECT_BRACKET :
                    snprintf(expected, sizeof(expected), " Bracket $%s Value ", name) ;
                    break ;
                default :
                    snprintf(expected, sizeof(expected), "Line%s", name) ;
                    break ;
            }

            CHECK(got && !strcmp(got, expected), "%s: key %s is [%s], expected [%s]", sec->header, name, got ? got : "(absent)", expected) ;
        }

        parse_store_free(&st) ;
    }
}

int main(void) {

    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    table.u.parser.id = E_PARSER_SECTION_STARTSTOP_EXEC ;

    basic(table) ;
    basic1(table) ;
    basic2(table) ;
    basic3(table) ;
    basic4(table) ;
    basic5(table) ;
    comment_inside(table) ;
    comment_outside(table) ;
    comment_outside_valid(table) ;
    odd_bracket(table) ;
    odd_bracket_validkey(table) ;
    no_bracket(table) ;
    nested_bracket(table) ;
    bash(table) ;
    bash_case(table) ;
    invalid_key(table) ;
    invalid_key1(table) ;
    eof(table) ;
    eof_invalid(table) ;
    valid_section(table) ;
    invalid_section(table) ;
    empty_bracket(table) ;

    PROG = "parse" ;
    frontend_cases_run() ;
    store_out_of_bounds() ;
    every_key_of_every_section() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}