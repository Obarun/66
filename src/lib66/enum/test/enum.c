#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <66/enum.h>
#include <66/enum_parser.h>
#include <66/enum_service.h>
#include <66/enum_tree.h>

// Helper function to count non-null entries in a key_description_t list
static size_t count_key_description_entries(key_description_t const *list) {
    size_t count = 0;
    while (list && list[count].name) count++;
    return count;
}

// Helper function to count non-null strings in a string array
static size_t count_string_array_entries(const char *array[]) {
    size_t count = 0;
    while (array && array[count]) count++;
    return count;
}

// Helper function to test enum_to_key for a single list
static void test_enum_to_key_list(key_description_t const *list, const char *list_name, const char *str_array[], int endofkey) {
    printf("Testing enum_to_key for %s...\n", list_name);
    size_t count = count_key_description_entries(list);
    assert(count == count_string_array_entries(str_array)); // Ensure list and string array match
    assert(count == (size_t)endofkey); // Ensure endofkey matches list length
    assert(get_endofkey(list) == endofkey); // Verify get_endofkey

    for (size_t i = 0; i < count; i++) {
        assert(list[i].id == (int)i); // IDs should match indices
        const char *result = enum_to_key(list, list[i].id);
        if (result == NULL || strcmp(result, str_array[i]) != 0) {
            fprintf(stderr, "Failed: enum_to_key(%s, %d) returned '%s', expected '%s'\n",
                    list_name, list[i].id, result ? result : "NULL", str_array[i]);
        }
        assert(result != NULL);
        assert(strcmp(result, str_array[i]) == 0);
        assert(strcmp(result, *list[i].name) == 0);
    }

    // Edge cases
    assert(enum_to_key(list, endofkey) == NULL); // Sentinel value
    assert(enum_to_key(list, -1) == NULL); // Negative index
    assert(enum_to_key(list, endofkey + 1) == NULL); // Out-of-bounds
    assert(enum_to_key(NULL, 0) == NULL); // NULL list
}

// Helper function to test key_to_enum for a single list
static void test_key_to_enum_list(key_description_t const *list, const char *list_name, const char *str_array[]) {
    printf("Testing key_to_enum for %s...\n", list_name);
    size_t count = count_key_description_entries(list);
    assert(count == count_string_array_entries(str_array));

    for (size_t i = 0; i < count; i++) {
        ssize_t result = key_to_enum(list, str_array[i]);
        assert(result == (ssize_t)i);
        assert(result == list[i].id);
    }

    // Edge cases
    assert(key_to_enum(list, "Invalid") == -1); // Invalid string
    assert(key_to_enum(NULL, str_array[0]) == -1); // NULL list
    assert(key_to_enum(list, NULL) == -1); // NULL key
}

// Test get_endofkey
void test_get_endofkey(void) {
    printf("Testing get_endofkey...\n");

    // Parser lists
    assert(get_endofkey(enum_list_parser_expected) == E_PARSER_EXPECT_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section) == E_PARSER_SECTION_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_main) == E_PARSER_SECTION_MAIN_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_startstop) == E_PARSER_SECTION_STARTSTOP_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_logger) == E_PARSER_SECTION_LOGGER_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_environ) == E_PARSER_SECTION_ENVIRON_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_regex) == E_PARSER_SECTION_REGEX_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_section_execute) == E_PARSER_SECTION_EXECUTE_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_io_type) == E_PARSER_IO_TYPE_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_type) == E_PARSER_TYPE_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_opts) == E_PARSER_OPTS_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_flags) == E_PARSER_FLAGS_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_build) == E_PARSER_BUILD_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_time) == E_PARSER_TIME_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_seed) == E_PARSER_SEED_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_mandatory) == E_PARSER_MANDATORY_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_caps) == E_PARSER_CAPS_ENDOFKEY);

    // Service lists
    assert(get_endofkey(enum_list_service_config) == E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    assert(get_endofkey(enum_list_service_path) == E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    assert(get_endofkey(enum_list_service_deps) == E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    assert(get_endofkey(enum_list_service_execute) == E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    assert(get_endofkey(enum_list_service_live) == E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    assert(get_endofkey(enum_list_service_environ) == E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    assert(get_endofkey(enum_list_service_regex) == E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    assert(get_endofkey(enum_list_service_io) == E_RESOLVE_SERVICE_IO_ENDOFKEY);
    assert(get_endofkey(enum_list_service_limit) == E_RESOLVE_SERVICE_LIMIT_ENDOFKEY);

    // Tree lists
    assert(get_endofkey(enum_list_tree) == E_RESOLVE_TREE_ENDOFKEY);
    assert(get_endofkey(enum_list_tree_master) == E_RESOLVE_TREE_MASTER_ENDOFKEY);

    // Edge cases
    assert(get_endofkey(NULL) == -1); // NULL list
    assert(get_endofkey((key_description_t const *)0xdeadbeef) == -1); // Invalid pointer
}

// Test enum_to_key for all lists
void test_enum_to_key(void) {
    // Parser lists
    test_enum_to_key_list(enum_list_parser_expected, "parser_expected", enum_str_parser_expected, E_PARSER_EXPECT_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section, "parser_section", enum_str_parser_section, E_PARSER_SECTION_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_main, "parser_section_main", enum_str_parser_section_main, E_PARSER_SECTION_MAIN_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_startstop, "parser_section_startstop", enum_str_parser_section_startstop, E_PARSER_SECTION_STARTSTOP_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_logger, "parser_section_logger", enum_str_parser_section_logger, E_PARSER_SECTION_LOGGER_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_environ, "parser_section_environ", enum_str_parser_section_environ, E_PARSER_SECTION_ENVIRON_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_regex, "parser_section_regex", enum_str_parser_section_regex, E_PARSER_SECTION_REGEX_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_section_execute, "parser_section_execute", enum_str_parser_section_execute, E_PARSER_SECTION_EXECUTE_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_io_type, "parser_io_type", enum_str_parser_io_type, E_PARSER_IO_TYPE_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_type, "parser_type", enum_str_parser_type, E_PARSER_TYPE_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_opts, "parser_opts", enum_str_parser_opts, E_PARSER_OPTS_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_flags, "parser_flags", enum_str_parser_flags, E_PARSER_FLAGS_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_build, "parser_build", enum_str_parser_build, E_PARSER_BUILD_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_time, "parser_time", enum_str_parser_time, E_PARSER_TIME_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_seed, "parser_seed", enum_str_parser_seed, E_PARSER_SEED_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_mandatory, "parser_mandatory", enum_str_parser_mandatory, E_PARSER_MANDATORY_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_caps, "parser_caps", enum_str_parser_caps, E_PARSER_CAPS_ENDOFKEY);

    // Service lists
    test_enum_to_key_list(enum_list_service_config, "service_config", enum_str_service_config, E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_path, "service_path", enum_str_service_path, E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_deps, "service_deps", enum_str_service_deps, E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_execute, "service_execute", enum_str_service_execute, E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_live, "service_live", enum_str_service_live, E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_environ, "service_environ", enum_str_service_environ, E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_regex, "service_regex", enum_str_service_regex, E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_io, "service_io", enum_str_service_io, E_RESOLVE_SERVICE_IO_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_limit, "service_limit", enum_str_service_limit, E_RESOLVE_SERVICE_LIMIT_ENDOFKEY);

    // Tree lists
    test_enum_to_key_list(enum_list_tree, "tree", enum_str_tree, E_RESOLVE_TREE_ENDOFKEY);
    test_enum_to_key_list(enum_list_tree_master, "tree_master", enum_str_tree_master, E_RESOLVE_TREE_MASTER_ENDOFKEY);
}

// Test key_to_enum for all lists
void test_key_to_enum(void) {
    // Parser lists
    test_key_to_enum_list(enum_list_parser_expected, "parser_expected", enum_str_parser_expected);
    test_key_to_enum_list(enum_list_parser_section, "parser_section", enum_str_parser_section);
    test_key_to_enum_list(enum_list_parser_section_main, "parser_section_main", enum_str_parser_section_main);
    test_key_to_enum_list(enum_list_parser_section_startstop, "parser_section_startstop", enum_str_parser_section_startstop);
    test_key_to_enum_list(enum_list_parser_section_logger, "parser_section_logger", enum_str_parser_section_logger);
    test_key_to_enum_list(enum_list_parser_section_environ, "parser_section_environ", enum_str_parser_section_environ);
    test_key_to_enum_list(enum_list_parser_section_regex, "parser_section_regex", enum_str_parser_section_regex);
    test_key_to_enum_list(enum_list_parser_section_execute, "parser_section_execute", enum_str_parser_section_execute);
    test_key_to_enum_list(enum_list_parser_io_type, "parser_io_type", enum_str_parser_io_type);
    test_key_to_enum_list(enum_list_parser_type, "parser_type", enum_str_parser_type);
    test_key_to_enum_list(enum_list_parser_opts, "parser_opts", enum_str_parser_opts);
    test_key_to_enum_list(enum_list_parser_flags, "parser_flags", enum_str_parser_flags);
    test_key_to_enum_list(enum_list_parser_build, "parser_build", enum_str_parser_build);
    test_key_to_enum_list(enum_list_parser_time, "parser_time", enum_str_parser_time);
    test_key_to_enum_list(enum_list_parser_seed, "parser_seed", enum_str_parser_seed);
    test_key_to_enum_list(enum_list_parser_mandatory, "parser_mandatory", enum_str_parser_mandatory);
    test_key_to_enum_list(enum_list_parser_caps, "parser_caps", enum_str_parser_caps);

    // Service lists
    test_key_to_enum_list(enum_list_service_config, "service_config", enum_str_service_config);
    test_key_to_enum_list(enum_list_service_path, "service_path", enum_str_service_path);
    test_key_to_enum_list(enum_list_service_deps, "service_deps", enum_str_service_deps);
    test_key_to_enum_list(enum_list_service_execute, "service_execute", enum_str_service_execute);
    test_key_to_enum_list(enum_list_service_live, "service_live", enum_str_service_live);
    test_key_to_enum_list(enum_list_service_environ, "service_environ", enum_str_service_environ);
    test_key_to_enum_list(enum_list_service_regex, "service_regex", enum_str_service_regex);
    test_key_to_enum_list(enum_list_service_io, "service_io", enum_str_service_io);
    test_key_to_enum_list(enum_list_service_limit, "service_limit", enum_str_service_limit);

    // Tree lists
    test_key_to_enum_list(enum_list_tree, "tree", enum_str_tree);
    test_key_to_enum_list(enum_list_tree_master, "tree_master", enum_str_tree_master);
}

// Test enum_get_list
void test_enum_get_list(void) {
    printf("Testing enum_get_list...\n");
    resolve_enum_table_t table;

    // Test PARSER category
    table.category = E_RESOLVE_CATEGORY_PARSER;
    table.u.parser.category = E_PARSER_CATEGORY_SECTION_MAIN;
    assert(enum_get_list(table) == enum_list_parser_section_main);
    table.u.parser.category = E_PARSER_CATEGORY_IO_TYPE;
    assert(enum_get_list(table) == enum_list_parser_io_type);
    table.u.parser.category = E_PARSER_CATEGORY_TIME;
    assert(enum_get_list(table) == enum_list_parser_time);

    // Test SERVICE category
    table.category = E_RESOLVE_CATEGORY_SERVICE;
    table.u.service.category = E_RESOLVE_SERVICE_CATEGORY_CONFIG;
    assert(enum_get_list(table) == enum_list_service_config);
    table.u.service.category = E_RESOLVE_SERVICE_CATEGORY_IO;
    assert(enum_get_list(table) == enum_list_service_io);

    // Test TREE category
    table.category = E_RESOLVE_CATEGORY_TREE;
    table.u.tree.category = E_RESOLVE_TREE_CATEGORY_TREE;
    assert(enum_get_list(table) == enum_list_tree);
    table.u.tree.category = E_RESOLVE_TREE_CATEGORY_MASTER;
    assert(enum_get_list(table) == enum_list_tree_master);

    // Edge case: Invalid category
    errno = 0;
    table.category = E_RESOLVE_CATEGORY_ENDOFKEY;
    assert(enum_get_list(table) == NULL);
    assert(errno == EINVAL);
}

// Test enum_get_list_parser
void test_enum_get_list_parser(void) {
    printf("Testing enum_get_list_parser...\n");
    resolve_parser_enum_table_t table;

    // Test all parser categories
    table.category = E_PARSER_CATEGORY_SECTION_MAIN;
    assert(enum_get_list_parser(table) == enum_list_parser_section_main);
    table.category = E_PARSER_CATEGORY_SECTION_STARTSTOP;
    assert(enum_get_list_parser(table) == enum_list_parser_section_startstop);
    table.category = E_PARSER_CATEGORY_SECTION_LOGGER;
    assert(enum_get_list_parser(table) == enum_list_parser_section_logger);
    table.category = E_PARSER_CATEGORY_SECTION_ENVIRON;
    assert(enum_get_list_parser(table) == enum_list_parser_section_environ);
    table.category = E_PARSER_CATEGORY_SECTION_REGEX;
    assert(enum_get_list_parser(table) == enum_list_parser_section_regex);
    table.category = E_PARSER_CATEGORY_SECTION_EXECUTE;
    assert(enum_get_list_parser(table) == enum_list_parser_section_execute);
    table.category = E_PARSER_CATEGORY_SECTION;
    assert(enum_get_list_parser(table) == enum_list_parser_section);
    table.category = E_PARSER_CATEGORY_IO_TYPE;
    assert(enum_get_list_parser(table) == enum_list_parser_io_type);
    table.category = E_PARSER_CATEGORY_TYPE;
    assert(enum_get_list_parser(table) == enum_list_parser_type);
    table.category = E_PARSER_CATEGORY_OPTS;
    assert(enum_get_list_parser(table) == enum_list_parser_opts);
    table.category = E_PARSER_CATEGORY_FLAGS;
    assert(enum_get_list_parser(table) == enum_list_parser_flags);
    table.category = E_PARSER_CATEGORY_BUILD;
    assert(enum_get_list_parser(table) == enum_list_parser_build);
    table.category = E_PARSER_CATEGORY_TIME;
    assert(enum_get_list_parser(table) == enum_list_parser_time);
    table.category = E_PARSER_CATEGORY_SEED;
    assert(enum_get_list_parser(table) == enum_list_parser_seed);
    table.category = E_PARSER_CATEGORY_EXPECTED;
    assert(enum_get_list_parser(table) == enum_list_parser_expected);
    table.category = E_PARSER_CATEGORY_MANDATORY;
    assert(enum_get_list_parser(table) == enum_list_parser_mandatory);
    table.category = E_PARSER_CATEGORY_CAPS;
    assert(enum_get_list_parser(table) == enum_list_parser_caps);

    // Edge case: Invalid category
    errno = 0;
    table.category = E_PARSER_CATEGORY_ENDOFKEY;
    assert(enum_get_list_parser(table) == NULL);
    assert(errno == EINVAL);
}

// Test enum_get_list_service
void test_enum_get_list_service(void) {
    printf("Testing enum_get_list_service...\n");
    resolve_service_enum_table_t table;

    // Test all service categories
    table.category = E_RESOLVE_SERVICE_CATEGORY_CONFIG;
    assert(enum_get_list_service(table) == enum_list_service_config);
    table.category = E_RESOLVE_SERVICE_CATEGORY_PATH;
    assert(enum_get_list_service(table) == enum_list_service_path);
    table.category = E_RESOLVE_SERVICE_CATEGORY_DEPS;
    assert(enum_get_list_service(table) == enum_list_service_deps);
    table.category = E_RESOLVE_SERVICE_CATEGORY_EXECUTE;
    assert(enum_get_list_service(table) == enum_list_service_execute);
    table.category = E_RESOLVE_SERVICE_CATEGORY_LIVE;
    assert(enum_get_list_service(table) == enum_list_service_live);
    table.category = E_RESOLVE_SERVICE_CATEGORY_ENVIRON;
    assert(enum_get_list_service(table) == enum_list_service_environ);
    table.category = E_RESOLVE_SERVICE_CATEGORY_REGEX;
    assert(enum_get_list_service(table) == enum_list_service_regex);
    table.category = E_RESOLVE_SERVICE_CATEGORY_EXECUTE;
    assert(enum_get_list_service(table) == enum_list_service_execute);
    table.category = E_RESOLVE_SERVICE_CATEGORY_IO;
    assert(enum_get_list_service(table) == enum_list_service_io);
    table.category = E_RESOLVE_SERVICE_CATEGORY_LIMIT;
    assert(enum_get_list_service(table) == enum_list_service_limit);

    // Edge case: Invalid category
    errno = 0;
    table.category = E_RESOLVE_SERVICE_CATEGORY_ENDOFKEY;
    assert(enum_get_list_service(table) == NULL);
    assert(errno == EINVAL);
}

// Test enum_get_list_tree
void test_enum_get_list_tree(void) {
    printf("Testing enum_get_list_tree...\n");
    resolve_tree_enum_table_t table;

    // Test tree and master categories
    table.category = E_RESOLVE_TREE_CATEGORY_TREE;
    assert(enum_get_list_tree(table) == enum_list_tree);
    table.category = E_RESOLVE_TREE_CATEGORY_MASTER;
    assert(enum_get_list_tree(table) == enum_list_tree_master);

    // Edge case: Invalid category
    errno = 0;
    table.category = E_RESOLVE_TREE_CATEGORY_ENDOFKEY;
    assert(enum_get_list_tree(table) == NULL);
    assert(errno == EINVAL);
}

// Test parser enums (all values)
void test_parser_enums(void) {
    printf("Testing parser enums...\n");
    unsigned int m = 0 ;
    // resolve_parser_enum_expected_t
    assert(E_PARSER_EXPECT_LINE == (m)++);
    assert(E_PARSER_EXPECT_BRACKET == (m)++);
    assert(E_PARSER_EXPECT_UINT == (m)++);
    assert(E_PARSER_EXPECT_SLASH == (m)++);
    assert(E_PARSER_EXPECT_QUOTE == (m)++);
    assert(E_PARSER_EXPECT_KEYVAL == (m)++);
    assert(E_PARSER_EXPECT_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_t
    assert(E_PARSER_SECTION_MAIN == (m)++);
    assert(E_PARSER_SECTION_START == (m)++);
    assert(E_PARSER_SECTION_STOP == (m)++);
    assert(E_PARSER_SECTION_LOGGER == (m)++);
    assert(E_PARSER_SECTION_ENVIRONMENT == (m)++);
    assert(E_PARSER_SECTION_REGEX == (m)++);
    assert(E_PARSER_SECTION_EXECUTE == (m)++);
    assert(E_PARSER_SECTION_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_main_t
    assert(E_PARSER_SECTION_MAIN_TYPE == (m)++);
    assert(E_PARSER_SECTION_MAIN_VERSION == (m)++);
    assert(E_PARSER_SECTION_MAIN_DESCRIPTION == (m)++);
    assert(E_PARSER_SECTION_MAIN_DEPENDS == (m)++);
    assert(E_PARSER_SECTION_MAIN_REQUIREDBY == (m)++);
    assert(E_PARSER_SECTION_MAIN_OPTSDEPS == (m)++);
    assert(E_PARSER_SECTION_MAIN_CONTENTS == (m)++);
    assert(E_PARSER_SECTION_MAIN_OPTIONS == (m)++);
    assert(E_PARSER_SECTION_MAIN_NOTIFY == (m)++);
    assert(E_PARSER_SECTION_MAIN_USER == (m)++);
    assert(E_PARSER_SECTION_MAIN_TIMESTART == (m)++);
    assert(E_PARSER_SECTION_MAIN_TIMESTOP == (m)++);
    assert(E_PARSER_SECTION_MAIN_DEATH == (m)++);
    assert(E_PARSER_SECTION_MAIN_DEATHTIME == (m)++);
    assert(E_PARSER_SECTION_MAIN_COPYFROM == (m)++);
    assert(E_PARSER_SECTION_MAIN_SIGNAL == (m)++);
    assert(E_PARSER_SECTION_MAIN_FLAGS == (m)++);
    assert(E_PARSER_SECTION_MAIN_INTREE == (m)++);
    assert(E_PARSER_SECTION_MAIN_STDIN == (m)++);
    assert(E_PARSER_SECTION_MAIN_STDOUT == (m)++);
    assert(E_PARSER_SECTION_MAIN_STDERR == (m)++);
    assert(E_PARSER_SECTION_MAIN_PROVIDE == (m)++);
    assert(E_PARSER_SECTION_MAIN_CONFLICT == (m)++);
    assert(E_PARSER_SECTION_MAIN_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_startstop_t
    assert(E_PARSER_SECTION_STARTSTOP_BUILD == (m)++);
    assert(E_PARSER_SECTION_STARTSTOP_RUNAS == (m)++);
    assert(E_PARSER_SECTION_STARTSTOP_EXEC == (m)++);
    assert(E_PARSER_SECTION_STARTSTOP_TIMESTART == (m)++);
    assert(E_PARSER_SECTION_STARTSTOP_TIMESTOP == (m)++);
    assert(E_PARSER_SECTION_STARTSTOP_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_logger_t
    assert(E_PARSER_SECTION_LOGGER_BUILD == (m)++);
    assert(E_PARSER_SECTION_LOGGER_RUNAS == (m)++);
    assert(E_PARSER_SECTION_LOGGER_EXEC == (m)++);
    assert(E_PARSER_SECTION_LOGGER_BACKUP == (m)++);
    assert(E_PARSER_SECTION_LOGGER_MAXSIZE == (m)++);
    assert(E_PARSER_SECTION_LOGGER_TIMESTAMP == (m)++);
    assert(E_PARSER_SECTION_LOGGER_TIMESTART == (m)++);
    assert(E_PARSER_SECTION_LOGGER_TIMESTOP == (m)++);
    assert(E_PARSER_SECTION_LOGGER_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_environ_t
    assert(E_PARSER_SECTION_ENVIRON_ENVAL == (m)++);
    assert(E_PARSER_SECTION_ENVIRON_IMPORTFILE == (m)++);
    assert(E_PARSER_SECTION_ENVIRON_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_regex_t
    assert(E_PARSER_SECTION_REGEX_CONFIGURE == (m)++);
    assert(E_PARSER_SECTION_REGEX_DIRECTORIES == (m)++);
    assert(E_PARSER_SECTION_REGEX_FILES == (m)++);
    assert(E_PARSER_SECTION_REGEX_INFILES == (m)++);
    assert(E_PARSER_SECTION_REGEX_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_section_execute_t
    assert(E_PARSER_SECTION_EXECUTE_LIMITAS == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITCORE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITCPU == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITDATA == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITFSIZE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITLOCKS == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITMEMLOCK == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITMSGQUEUE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITNICE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITNOFILE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITNPROC == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITRTPRIO == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITRTTIME == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITSIGPENDING == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_LIMITSTACK == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_BLOCK_PRIVILEGES == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_UMASK == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_NICE == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_CHDIR == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_CAPS_BOUND == (m)++);
    assert(E_PARSER_SECTION_EXECUTE_CAPS_AMBIENT == (m)++);

    m = 0 ;
    // resolve_parser_enum_io_type_t
    assert(E_PARSER_IO_TYPE_TTY == (m)++);
    assert(E_PARSER_IO_TYPE_FILE == (m)++);
    assert(E_PARSER_IO_TYPE_CONSOLE == (m)++);
    assert(E_PARSER_IO_TYPE_66LOG == (m)++);
    assert(E_PARSER_IO_TYPE_SYSLOG == (m)++);
    assert(E_PARSER_IO_TYPE_INHERIT == (m)++);
    assert(E_PARSER_IO_TYPE_NULL == (m)++);
    assert(E_PARSER_IO_TYPE_PARENT == (m)++);
    assert(E_PARSER_IO_TYPE_CLOSE == (m)++);
    assert(E_PARSER_IO_TYPE_NOTSET == (m)++);
    assert(E_PARSER_IO_TYPE_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_type_t
    assert(E_PARSER_TYPE_CLASSIC == (m)++);
    assert(E_PARSER_TYPE_ONESHOT == (m)++);
    assert(E_PARSER_TYPE_MODULE == (m)++);
    assert(E_PARSER_TYPE_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_opts_t
    assert(E_PARSER_OPTS_LOGGER == (m)++);
    assert(E_PARSER_OPTS_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_flags_t
    assert(E_PARSER_FLAGS_DOWN == (m)++);
    assert(E_PARSER_FLAGS_EARLIER == (m)++);
    assert(E_PARSER_FLAGS_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_build_t
    assert(E_PARSER_BUILD_AUTO == (m)++);
    assert(E_PARSER_BUILD_CUSTOM == (m)++);
    assert(E_PARSER_BUILD_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_time_t
    assert(E_PARSER_TIME_TAI == (m)++);
    assert(E_PARSER_TIME_ISO == (m)++);
    assert(E_PARSER_TIME_NONE == (m)++);
    assert(E_PARSER_TIME_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_seed_t
    assert(E_PARSER_SEED_DEPENDS == (m)++);
    assert(E_PARSER_SEED_REQUIREDBY == (m)++);
    assert(E_PARSER_SEED_ENABLE == (m)++);
    assert(E_PARSER_SEED_ALLOW == (m)++);
    assert(E_PARSER_SEED_DENY == (m)++);
    assert(E_PARSER_SEED_CURRENT == (m)++);
    assert(E_PARSER_SEED_GROUPS == (m)++);
    assert(E_PARSER_SEED_CONTENTS == (m)++);
    assert(E_PARSER_SEED_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_mandatory_t
    assert(E_PARSER_MANDATORY_NEED == (m)++);
    assert(E_PARSER_MANDATORY_OPTS == (m)++);
    assert(E_PARSER_MANDATORY_CUSTOM == (m)++);
    assert(E_PARSER_MANDATORY_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_parser_enum_category_t
    assert(E_PARSER_CATEGORY_SECTION_MAIN == (m)++);
    assert(E_PARSER_CATEGORY_SECTION_STARTSTOP == (m)++);
    assert(E_PARSER_CATEGORY_SECTION_LOGGER == (m)++);
    assert(E_PARSER_CATEGORY_SECTION_ENVIRON == (m)++);
    assert(E_PARSER_CATEGORY_SECTION_REGEX == (m)++);
    assert(E_PARSER_CATEGORY_SECTION_EXECUTE == (m)++);
    assert(E_PARSER_CATEGORY_SECTION == (m)++);
    assert(E_PARSER_CATEGORY_IO_TYPE == (m)++);
    assert(E_PARSER_CATEGORY_TYPE == (m)++);
    assert(E_PARSER_CATEGORY_OPTS == (m)++);
    assert(E_PARSER_CATEGORY_FLAGS == (m)++);
    assert(E_PARSER_CATEGORY_BUILD == (m)++);
    assert(E_PARSER_CATEGORY_TIME == (m)++);
    assert(E_PARSER_CATEGORY_SEED == (m)++);
    assert(E_PARSER_CATEGORY_EXPECTED == (m)++);
    assert(E_PARSER_CATEGORY_MANDATORY == (m)++);
    assert(E_PARSER_CATEGORY_CAPS == (m)++);
    assert(E_PARSER_CATEGORY_ENDOFKEY == (m)++);
}

// Test service enums (all values)
void test_service_enums(void) {
    printf("Testing service enums...\n");

    unsigned int m = 0 ;
    // resolve_service_enum_config_t
    assert(E_RESOLVE_SERVICE_CONFIG_RVERSION == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_NAME == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_DESCRIPTION == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_VERSION == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_TYPE == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_EARLIER == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_COPYFROM == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_INTREE == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_OWNERSTR == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_OWNER == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_TREENAME == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_USER == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_INNS == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_ENABLED == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_ISLOG == (m)++);
    assert(E_RESOLVE_SERVICE_CONFIG_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_path_t
    assert(E_RESOLVE_SERVICE_PATH_HOME == (m)++);
    assert(E_RESOLVE_SERVICE_PATH_FRONTEND == (m)++);
    assert(E_RESOLVE_SERVICE_PATH_SERVICEDIR == (m)++);
    assert(E_RESOLVE_SERVICE_PATH_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_deps_t
    assert(E_RESOLVE_SERVICE_DEPS_DEPENDS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_REQUIREDBY == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_OPTSDEPS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_CONTENTS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_PROVIDE == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_CONFLICT == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NDEPENDS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NREQUIREDBY == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NOPTSDEPS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NCONTENTS == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NPROVIDE == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_NCONFLICT == (m)++);
    assert(E_RESOLVE_SERVICE_DEPS_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_execute_t
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_USER == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_USER == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_DOWN == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_BLOCK_PRIVILEGES == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_UMASK == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_WANT_UMASK == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_NICE == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_WANT_NICE == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_CHDIR == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_CAPS_BOUND == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_CAPS_AMBIENT == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_NOTIFY == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_MAXDEATH == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_MAXDEATHTIME == (m)++);
    assert(E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_live_t
    assert(E_RESOLVE_SERVICE_LIVE_LIVEDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_STATUS == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_SERVICEDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_SCANDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_STATEDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_EVENTDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR == (m)++);
    assert(E_RESOLVE_SERVICE_LIVE_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_environ_t
    assert(E_RESOLVE_SERVICE_ENVIRON_ENV == (m)++);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENVDIR == (m)++);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE == (m)++);
    assert(E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE == (m)++);
    assert(E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE == (m)++);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_regex_t
    assert(E_RESOLVE_SERVICE_REGEX_CONFIGURE == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_DIRECTORIES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_FILES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_INFILES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_NDIRECTORIES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_NFILES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_NINFILES == (m)++);
    assert(E_RESOLVE_SERVICE_REGEX_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_io_t
    assert(E_RESOLVE_SERVICE_IO_STDIN == (m)++);
    assert(E_RESOLVE_SERVICE_IO_STDINDEST == (m)++);
    assert(E_RESOLVE_SERVICE_IO_STDOUT == (m)++);
    assert(E_RESOLVE_SERVICE_IO_STDOUTDEST == (m)++);
    assert(E_RESOLVE_SERVICE_IO_STDERR == (m)++);
    assert(E_RESOLVE_SERVICE_IO_STDERRDEST == (m)++);
    assert(E_RESOLVE_SERVICE_IO_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_service_enum_category_t
    assert(E_RESOLVE_SERVICE_CATEGORY_CONFIG == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_PATH == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_DEPS == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_EXECUTE == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_LIVE == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_ENVIRON == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_REGEX == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_IO == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_LIMIT == (m)++);
    assert(E_RESOLVE_SERVICE_CATEGORY_ENDOFKEY == (m)++);
}

// Test tree enums (all values)
void test_tree_enums(void) {
    printf("Testing tree enums...\n");

    unsigned int m = 0 ;
    // resolve_tree_enum_t
    assert(E_RESOLVE_TREE_RVERSION == (m)++);
    assert(E_RESOLVE_TREE_NAME == (m)++);
    assert(E_RESOLVE_TREE_ENABLED == (m)++);
    assert(E_RESOLVE_TREE_DEPENDS == (m)++);
    assert(E_RESOLVE_TREE_REQUIREDBY == (m)++);
    assert(E_RESOLVE_TREE_ALLOW == (m)++);
    assert(E_RESOLVE_TREE_GROUPS == (m)++);
    assert(E_RESOLVE_TREE_CONTENTS == (m)++);
    assert(E_RESOLVE_TREE_NDEPENDS == (m)++);
    assert(E_RESOLVE_TREE_NREQUIREDBY == (m)++);
    assert(E_RESOLVE_TREE_NALLOW == (m)++);
    assert(E_RESOLVE_TREE_NGROUPS == (m)++);
    assert(E_RESOLVE_TREE_NCONTENTS == (m)++);
    assert(E_RESOLVE_TREE_INIT == (m)++);
    assert(E_RESOLVE_TREE_SUPERVISED == (m)++);
    assert(E_RESOLVE_TREE_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_tree_master_enum_t
    assert(E_RESOLVE_TREE_MASTER_RVERSION == (m)++);
    assert(E_RESOLVE_TREE_MASTER_NAME == (m)++);
    assert(E_RESOLVE_TREE_MASTER_ALLOW == (m)++);
    assert(E_RESOLVE_TREE_MASTER_CURRENT == (m)++);
    assert(E_RESOLVE_TREE_MASTER_CONTENTS == (m)++);
    assert(E_RESOLVE_TREE_MASTER_NALLOW == (m)++);
    assert(E_RESOLVE_TREE_MASTER_NCONTENTS == (m)++);
    assert(E_RESOLVE_TREE_MASTER_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_tree_enum_category_t
    assert(E_RESOLVE_TREE_CATEGORY_TREE == (m)++);
    assert(E_RESOLVE_TREE_CATEGORY_MASTER == (m)++);
    assert(E_RESOLVE_TREE_CATEGORY_ENDOFKEY == (m)++);

    m = 0 ;
    // resolve_enum_category_t
    assert(E_RESOLVE_CATEGORY_PARSER == (m)++);
    assert(E_RESOLVE_CATEGORY_SERVICE == (m)++);
    assert(E_RESOLVE_CATEGORY_TREE == (m)++);
    assert(E_RESOLVE_CATEGORY_ENDOFKEY == (m)++);
}

// Test parser lists (all entries)
void test_parser_lists(void) {
    printf("Testing parser lists...\n");

    // enum_list_parser_expected
    assert(count_key_description_entries(enum_list_parser_expected) == E_PARSER_EXPECT_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_expected) == E_PARSER_EXPECT_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_EXPECT_ENDOFKEY; i++) {
        assert(enum_list_parser_expected[i].id == (int)i);
        assert(strcmp(*enum_list_parser_expected[i].name, enum_str_parser_expected[i]) == 0);
        assert(enum_list_parser_expected[i].expected == 0);
    }

    // enum_list_parser_section
    assert(count_key_description_entries(enum_list_parser_section) == E_PARSER_SECTION_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section) == E_PARSER_SECTION_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_ENDOFKEY; i++) {
        assert(enum_list_parser_section[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section[i].name, enum_str_parser_section[i]) == 0);
        assert(enum_list_parser_section[i].expected == 0);
    }

    // enum_list_parser_section_main
    assert(count_key_description_entries(enum_list_parser_section_main) == E_PARSER_SECTION_MAIN_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section_main) == E_PARSER_SECTION_MAIN_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_MAIN_ENDOFKEY; i++) {
        assert(enum_list_parser_section_main[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_main[i].name, enum_str_parser_section_main[i]) == 0);
        // Check expected values (from SECTION_MAIN_TEMPLATE)
        int expected_values[] = {
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_QUOTE, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_UINT,
            E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE,
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET
        };
        assert(enum_list_parser_section_main[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_startstop
    assert(count_key_description_entries(enum_list_parser_section_startstop) == E_PARSER_SECTION_STARTSTOP_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section_startstop) == E_PARSER_SECTION_STARTSTOP_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_STARTSTOP_ENDOFKEY; i++) {
        assert(enum_list_parser_section_startstop[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_startstop[i].name, enum_str_parser_section_startstop[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT };
        assert(enum_list_parser_section_startstop[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_logger
    assert(count_key_description_entries(enum_list_parser_section_logger) == E_PARSER_SECTION_LOGGER_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section_logger) == E_PARSER_SECTION_LOGGER_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_LOGGER_ENDOFKEY; i++) {
        assert(enum_list_parser_section_logger[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_logger[i].name, enum_str_parser_section_logger[i]) == 0);
        int expected_values[] = {
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_UINT,
            E_PARSER_EXPECT_UINT
        };
        assert(enum_list_parser_section_logger[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_environ
    assert(count_key_description_entries(enum_list_parser_section_environ) == E_PARSER_SECTION_ENVIRON_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section_environ) == E_PARSER_SECTION_ENVIRON_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_ENVIRON_ENDOFKEY; i++) {
        assert(enum_list_parser_section_environ[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_environ[i].name, enum_str_parser_section_environ[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_KEYVAL, E_PARSER_EXPECT_BRACKET };
        assert(enum_list_parser_section_environ[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_regex
    assert(count_key_description_entries(enum_list_parser_section_regex) == E_PARSER_SECTION_REGEX_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_section_regex) == E_PARSER_SECTION_REGEX_ENDOFKEY);
    for (size_t i = 0; i < E_PARSER_SECTION_REGEX_ENDOFKEY; i++) {
        assert(enum_list_parser_section_regex[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_regex[i].name, enum_str_parser_section_regex[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_QUOTE, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET };
        assert(enum_list_parser_section_regex[i].expected == expected_values[i]);
    }

    // enum_list_parser_io_type
    assert(count_key_description_entries(enum_list_parser_io_type) == E_PARSER_IO_TYPE_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_io_type) == E_PARSER_IO_TYPE_ENDOFKEY);
    for (size_t i = 0; i < 10; i++) {
        assert(enum_list_parser_io_type[i].id == (int)i);
        assert(strcmp(*enum_list_parser_io_type[i].name, enum_str_parser_io_type[i]) == 0);
        assert(enum_list_parser_io_type[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_type
    assert(count_key_description_entries(enum_list_parser_type) == E_PARSER_TYPE_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_type) == E_PARSER_TYPE_ENDOFKEY);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_type[i].id == (int)i);
        assert(strcmp(*enum_list_parser_type[i].name, enum_str_parser_type[i]) == 0);
        assert(enum_list_parser_type[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_opts
    assert(count_key_description_entries(enum_list_parser_opts) == E_PARSER_OPTS_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_opts) == E_PARSER_OPTS_ENDOFKEY);
    assert(enum_list_parser_opts[0].id == 0);
    assert(strcmp(*enum_list_parser_opts[0].name, enum_str_parser_opts[0]) == 0);
    assert(enum_list_parser_opts[0].expected == E_PARSER_EXPECT_BRACKET);

    // enum_list_parser_flags
    assert(count_key_description_entries(enum_list_parser_flags) == E_PARSER_FLAGS_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_flags) == E_PARSER_FLAGS_ENDOFKEY);
    for (size_t i = 0; i < 2; i++) {
        assert(enum_list_parser_flags[i].id == (int)i);
        assert(strcmp(*enum_list_parser_flags[i].name, enum_str_parser_flags[i]) == 0);
        assert(enum_list_parser_flags[i].expected == E_PARSER_EXPECT_BRACKET);
    }

    // enum_list_parser_build
    assert(count_key_description_entries(enum_list_parser_build) == E_PARSER_BUILD_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_build) == E_PARSER_BUILD_ENDOFKEY);
    for (size_t i = 0; i < 2; i++) {
        assert(enum_list_parser_build[i].id == (int)i);
        assert(strcmp(*enum_list_parser_build[i].name, enum_str_parser_build[i]) == 0);
        assert(enum_list_parser_build[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_time
    assert(count_key_description_entries(enum_list_parser_time) == E_PARSER_TIME_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_time) == E_PARSER_TIME_ENDOFKEY);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_time[i].id == (int)i);
        assert(strcmp(*enum_list_parser_time[i].name, enum_str_parser_time[i]) == 0);
        assert(enum_list_parser_time[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_seed
    assert(count_key_description_entries(enum_list_parser_seed) == E_PARSER_SEED_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_seed) == E_PARSER_SEED_ENDOFKEY);
    for (size_t i = 0; i < 8; i++) {
        assert(enum_list_parser_seed[i].id == (int)i);
        assert(strcmp(*enum_list_parser_seed[i].name, enum_str_parser_seed[i]) == 0);
        assert(enum_list_parser_seed[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_mandatory
    assert(count_key_description_entries(enum_list_parser_mandatory) == E_PARSER_MANDATORY_ENDOFKEY);
    assert(count_string_array_entries(enum_str_parser_mandatory) == E_PARSER_MANDATORY_ENDOFKEY);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_mandatory[i].id == (int)i);
        assert(strcmp(*enum_list_parser_mandatory[i].name, enum_str_parser_mandatory[i]) == 0);
        assert(enum_list_parser_mandatory[i].expected == 0);
    }
}

// Test service lists (all entries)
void test_service_lists(void) {
    printf("Testing service lists...\n");

    // enum_list_service_config
    assert(count_key_description_entries(enum_list_service_config) == E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_config) == E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    for (size_t i = 0; i < E_RESOLVE_SERVICE_CONFIG_ENDOFKEY; i++) {
        assert(enum_list_service_config[i].id == (int)i);
        assert(strcmp(*enum_list_service_config[i].name, enum_str_service_config[i]) == 0);
        assert(enum_list_service_config[i].expected == 0); // No expected field defined
    }

    // enum_list_service_path
    assert(count_key_description_entries(enum_list_service_path) == E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_path) == E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_service_path[i].id == (int)i);
        assert(strcmp(*enum_list_service_path[i].name, enum_str_service_path[i]) == 0);
        assert(enum_list_service_path[i].expected == 0);
    }

    // enum_list_service_deps
    assert(count_key_description_entries(enum_list_service_deps) == E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_deps) == E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    for (size_t i = 0; i < 10; i++) {
        assert(enum_list_service_deps[i].id == (int)i);
        assert(strcmp(*enum_list_service_deps[i].name, enum_str_service_deps[i]) == 0);
        assert(enum_list_service_deps[i].expected == 0);
    }

    // enum_list_service_execute
    assert(count_key_description_entries(enum_list_service_execute) == E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_execute) == E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    for (size_t i = 0; i < E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY; i++) {
        assert(enum_list_service_execute[i].id == (int)i);
        assert(strcmp(*enum_list_service_execute[i].name, enum_str_service_execute[i]) == 0);
        assert(enum_list_service_execute[i].expected == 0);
    }

    // enum_list_service_live
    assert(count_key_description_entries(enum_list_service_live) == E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_live) == E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    for (size_t i = 0; i < E_RESOLVE_SERVICE_LIVE_ENDOFKEY; i++) {
        assert(enum_list_service_live[i].id == (int)i);
        assert(strcmp(*enum_list_service_live[i].name, enum_str_service_live[i]) == 0);
        assert(enum_list_service_live[i].expected == 0);
    }

    // enum_list_service_environ
    assert(count_key_description_entries(enum_list_service_environ) == E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_environ) == E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    for (size_t i = 0; i < 5; i++) {
        assert(enum_list_service_environ[i].id == (int)i);
        assert(strcmp(*enum_list_service_environ[i].name, enum_str_service_environ[i]) == 0);
        assert(enum_list_service_environ[i].expected == 0);
    }

    // enum_list_service_regex
    assert(count_key_description_entries(enum_list_service_regex) == E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_regex) == E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    for (size_t i = 0; i < 7; i++) {
        assert(enum_list_service_regex[i].id == (int)i);
        assert(strcmp(*enum_list_service_regex[i].name, enum_str_service_regex[i]) == 0);
        assert(enum_list_service_regex[i].expected == 0);
    }

    // enum_list_service_io
    assert(count_key_description_entries(enum_list_service_io) == E_RESOLVE_SERVICE_IO_ENDOFKEY);
    assert(count_string_array_entries(enum_str_service_io) == E_RESOLVE_SERVICE_IO_ENDOFKEY);
    for (size_t i = 0; i < 6; i++) {
        assert(enum_list_service_io[i].id == (int)i);
        assert(strcmp(*enum_list_service_io[i].name, enum_str_service_io[i]) == 0);
        assert(enum_list_service_io[i].expected == 0);
    }
}

// Test tree lists (all entries)
void test_tree_lists(void) {
    printf("Testing tree lists...\n");

    // enum_list_tree
    assert(count_key_description_entries(enum_list_tree) == E_RESOLVE_TREE_ENDOFKEY);
    assert(count_string_array_entries(enum_str_tree) == E_RESOLVE_TREE_ENDOFKEY);
    for (size_t i = 0; i < 15; i++) {
        assert(enum_list_tree[i].id == (int)i);
        assert(strcmp(*enum_list_tree[i].name, enum_str_tree[i]) == 0);
        assert(enum_list_tree[i].expected == 0);
    }

    // enum_list_tree_master
    assert(count_key_description_entries(enum_list_tree_master) == E_RESOLVE_TREE_MASTER_ENDOFKEY);
    assert(count_string_array_entries(enum_str_tree_master) == E_RESOLVE_TREE_MASTER_ENDOFKEY);
    for (size_t i = 0; i < 7; i++) {
        assert(enum_list_tree_master[i].id == (int)i);
        assert(strcmp(*enum_list_tree_master[i].name, enum_str_tree_master[i]) == 0);
        assert(enum_list_tree_master[i].expected == 0);
    }
}

int main(void) {
    printf("Running tests...\n");

    test_get_endofkey();
    test_enum_to_key();
    test_key_to_enum();
    test_enum_get_list();
    test_enum_get_list_parser();
    test_enum_get_list_service();
    test_enum_get_list_tree();
    test_parser_enums();
    test_service_enums();
    test_tree_enums();
    test_parser_lists();
    test_service_lists();
    test_tree_lists();

    printf("All tests passed!\n");
    return 0;
}