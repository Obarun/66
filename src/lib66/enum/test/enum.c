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
    assert(get_endofkey(enum_list_parser_io_type) == E_PARSER_IO_TYPE_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_type) == E_PARSER_TYPE_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_opts) == E_PARSER_OPTS_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_flags) == E_PARSER_FLAGS_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_build) == E_PARSER_BUILD_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_time) == E_PARSER_TIME_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_seed) == E_PARSER_SEED_ENDOFKEY);
    assert(get_endofkey(enum_list_parser_mandatory) == E_PARSER_MANDATORY_ENDOFKEY);

    // Service lists
    assert(get_endofkey(enum_list_service_config) == E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    assert(get_endofkey(enum_list_service_path) == E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    assert(get_endofkey(enum_list_service_deps) == E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    assert(get_endofkey(enum_list_service_execute) == E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    assert(get_endofkey(enum_list_service_live) == E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    assert(get_endofkey(enum_list_service_logger) == E_RESOLVE_SERVICE_LOGGER_ENDOFKEY);
    assert(get_endofkey(enum_list_service_environ) == E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    assert(get_endofkey(enum_list_service_regex) == E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    assert(get_endofkey(enum_list_service_io) == E_RESOLVE_SERVICE_IO_ENDOFKEY);

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
    test_enum_to_key_list(enum_list_parser_io_type, "parser_io_type", enum_str_parser_io_type, E_PARSER_IO_TYPE_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_type, "parser_type", enum_str_parser_type, E_PARSER_TYPE_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_opts, "parser_opts", enum_str_parser_opts, E_PARSER_OPTS_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_flags, "parser_flags", enum_str_parser_flags, E_PARSER_FLAGS_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_build, "parser_build", enum_str_parser_build, E_PARSER_BUILD_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_time, "parser_time", enum_str_parser_time, E_PARSER_TIME_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_seed, "parser_seed", enum_str_parser_seed, E_PARSER_SEED_ENDOFKEY);
    test_enum_to_key_list(enum_list_parser_mandatory, "parser_mandatory", enum_str_parser_mandatory, E_PARSER_MANDATORY_ENDOFKEY);

    // Service lists
    test_enum_to_key_list(enum_list_service_config, "service_config", enum_str_service_config, E_RESOLVE_SERVICE_CONFIG_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_path, "service_path", enum_str_service_path, E_RESOLVE_SERVICE_PATH_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_deps, "service_deps", enum_str_service_deps, E_RESOLVE_SERVICE_DEPS_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_execute, "service_execute", enum_str_service_execute, E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_live, "service_live", enum_str_service_live, E_RESOLVE_SERVICE_LIVE_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_logger, "service_logger", enum_str_service_logger, E_RESOLVE_SERVICE_LOGGER_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_environ, "service_environ", enum_str_service_environ, E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_regex, "service_regex", enum_str_service_regex, E_RESOLVE_SERVICE_REGEX_ENDOFKEY);
    test_enum_to_key_list(enum_list_service_io, "service_io", enum_str_service_io, E_RESOLVE_SERVICE_IO_ENDOFKEY);

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
    test_key_to_enum_list(enum_list_parser_io_type, "parser_io_type", enum_str_parser_io_type);
    test_key_to_enum_list(enum_list_parser_type, "parser_type", enum_str_parser_type);
    test_key_to_enum_list(enum_list_parser_opts, "parser_opts", enum_str_parser_opts);
    test_key_to_enum_list(enum_list_parser_flags, "parser_flags", enum_str_parser_flags);
    test_key_to_enum_list(enum_list_parser_build, "parser_build", enum_str_parser_build);
    test_key_to_enum_list(enum_list_parser_time, "parser_time", enum_str_parser_time);
    test_key_to_enum_list(enum_list_parser_seed, "parser_seed", enum_str_parser_seed);
    test_key_to_enum_list(enum_list_parser_mandatory, "parser_mandatory", enum_str_parser_mandatory);

    // Service lists
    test_key_to_enum_list(enum_list_service_config, "service_config", enum_str_service_config);
    test_key_to_enum_list(enum_list_service_path, "service_path", enum_str_service_path);
    test_key_to_enum_list(enum_list_service_deps, "service_deps", enum_str_service_deps);
    test_key_to_enum_list(enum_list_service_execute, "service_execute", enum_str_service_execute);
    test_key_to_enum_list(enum_list_service_live, "service_live", enum_str_service_live);
    test_key_to_enum_list(enum_list_service_logger, "service_logger", enum_str_service_logger);
    test_key_to_enum_list(enum_list_service_environ, "service_environ", enum_str_service_environ);
    test_key_to_enum_list(enum_list_service_regex, "service_regex", enum_str_service_regex);
    test_key_to_enum_list(enum_list_service_io, "service_io", enum_str_service_io);

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
    table.u.service.category = E_RESOLVE_SERVICE_CATEGORY_LOGGER;
    assert(enum_get_list(table) == enum_list_service_logger);

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
    table.category = E_RESOLVE_SERVICE_CATEGORY_LOGGER;
    assert(enum_get_list_service(table) == enum_list_service_logger);
    table.category = E_RESOLVE_SERVICE_CATEGORY_ENVIRON;
    assert(enum_get_list_service(table) == enum_list_service_environ);
    table.category = E_RESOLVE_SERVICE_CATEGORY_REGEX;
    assert(enum_get_list_service(table) == enum_list_service_regex);
    table.category = E_RESOLVE_SERVICE_CATEGORY_IO;
    assert(enum_get_list_service(table) == enum_list_service_io);

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

    // resolve_parser_enum_expected_t
    assert(E_PARSER_EXPECT_LINE == 0);
    assert(E_PARSER_EXPECT_BRACKET == 1);
    assert(E_PARSER_EXPECT_UINT == 2);
    assert(E_PARSER_EXPECT_SLASH == 3);
    assert(E_PARSER_EXPECT_QUOTE == 4);
    assert(E_PARSER_EXPECT_KEYVAL == 5);
    assert(E_PARSER_EXPECT_ENDOFKEY == 6);

    // resolve_parser_enum_section_t
    assert(E_PARSER_SECTION_MAIN == 0);
    assert(E_PARSER_SECTION_START == 1);
    assert(E_PARSER_SECTION_STOP == 2);
    assert(E_PARSER_SECTION_LOGGER == 3);
    assert(E_PARSER_SECTION_ENVIRONMENT == 4);
    assert(E_PARSER_SECTION_REGEX == 5);
    assert(E_PARSER_SECTION_ENDOFKEY == 6);

    // resolve_parser_enum_section_main_t
    assert(E_PARSER_SECTION_MAIN_TYPE == 0);
    assert(E_PARSER_SECTION_MAIN_VERSION == 1);
    assert(E_PARSER_SECTION_MAIN_DESCRIPTION == 2);
    assert(E_PARSER_SECTION_MAIN_DEPENDS == 3);
    assert(E_PARSER_SECTION_MAIN_REQUIREDBY == 4);
    assert(E_PARSER_SECTION_MAIN_OPTSDEPS == 5);
    assert(E_PARSER_SECTION_MAIN_CONTENTS == 6);
    assert(E_PARSER_SECTION_MAIN_OPTIONS == 7);
    assert(E_PARSER_SECTION_MAIN_NOTIFY == 8);
    assert(E_PARSER_SECTION_MAIN_USER == 9);
    assert(E_PARSER_SECTION_MAIN_TIMESTART == 10);
    assert(E_PARSER_SECTION_MAIN_TIMESTOP == 11);
    assert(E_PARSER_SECTION_MAIN_DEATH == 12);
    assert(E_PARSER_SECTION_MAIN_COPYFROM == 13);
    assert(E_PARSER_SECTION_MAIN_SIGNAL == 14);
    assert(E_PARSER_SECTION_MAIN_FLAGS == 15);
    assert(E_PARSER_SECTION_MAIN_INTREE == 16);
    assert(E_PARSER_SECTION_MAIN_STDIN == 17);
    assert(E_PARSER_SECTION_MAIN_STDOUT == 18);
    assert(E_PARSER_SECTION_MAIN_STDERR == 19);
    assert(E_PARSER_SECTION_MAIN_ENDOFKEY == 20);

    // resolve_parser_enum_section_startstop_t
    assert(E_PARSER_SECTION_STARTSTOP_BUILD == 0);
    assert(E_PARSER_SECTION_STARTSTOP_RUNAS == 1);
    assert(E_PARSER_SECTION_STARTSTOP_EXEC == 2);
    assert(E_PARSER_SECTION_STARTSTOP_ENDOFKEY == 3);

    // resolve_parser_enum_section_logger_t
    assert(E_PARSER_SECTION_LOGGER_BUILD == 0);
    assert(E_PARSER_SECTION_LOGGER_RUNAS == 1);
    assert(E_PARSER_SECTION_LOGGER_EXEC == 2);
    assert(E_PARSER_SECTION_LOGGER_DESTINATION == 3);
    assert(E_PARSER_SECTION_LOGGER_BACKUP == 4);
    assert(E_PARSER_SECTION_LOGGER_MAXSIZE == 5);
    assert(E_PARSER_SECTION_LOGGER_TIMESTAMP == 6);
    assert(E_PARSER_SECTION_LOGGER_TIMESTART == 7);
    assert(E_PARSER_SECTION_LOGGER_TIMESTOP == 8);
    assert(E_PARSER_SECTION_LOGGER_ENDOFKEY == 9);

    // resolve_parser_enum_section_environ_t
    assert(E_PARSER_SECTION_ENVIRON_ENVAL == 0);
    assert(E_PARSER_SECTION_ENVIRON_IMPORTFILE == 1);
    assert(E_PARSER_SECTION_ENVIRON_ENDOFKEY == 2);

    // resolve_parser_enum_section_regex_t
    assert(E_PARSER_SECTION_REGEX_CONFIGURE == 0);
    assert(E_PARSER_SECTION_REGEX_DIRECTORIES == 1);
    assert(E_PARSER_SECTION_REGEX_FILES == 2);
    assert(E_PARSER_SECTION_REGEX_INFILES == 3);
    assert(E_PARSER_SECTION_REGEX_ENDOFKEY == 4);

    // resolve_parser_enum_io_type_t
    assert(E_PARSER_IO_TYPE_TTY == 0);
    assert(E_PARSER_IO_TYPE_FILE == 1);
    assert(E_PARSER_IO_TYPE_CONSOLE == 2);
    assert(E_PARSER_IO_TYPE_S6LOG == 3);
    assert(E_PARSER_IO_TYPE_SYSLOG == 4);
    assert(E_PARSER_IO_TYPE_INHERIT == 5);
    assert(E_PARSER_IO_TYPE_NULL == 6);
    assert(E_PARSER_IO_TYPE_PARENT == 7);
    assert(E_PARSER_IO_TYPE_CLOSE == 8);
    assert(E_PARSER_IO_TYPE_NOTSET == 9);
    assert(E_PARSER_IO_TYPE_ENDOFKEY == 10);

    // resolve_parser_enum_type_t
    assert(E_PARSER_TYPE_CLASSIC == 0);
    assert(E_PARSER_TYPE_ONESHOT == 1);
    assert(E_PARSER_TYPE_MODULE == 2);
    assert(E_PARSER_TYPE_ENDOFKEY == 3);

    // resolve_parser_enum_opts_t
    assert(E_PARSER_OPTS_LOGGER == 0);
    assert(E_PARSER_OPTS_ENDOFKEY == 1);

    // resolve_parser_enum_flags_t
    assert(E_PARSER_FLAGS_DOWN == 0);
    assert(E_PARSER_FLAGS_EARLIER == 1);
    assert(E_PARSER_FLAGS_ENDOFKEY == 2);

    // resolve_parser_enum_build_t
    assert(E_PARSER_BUILD_AUTO == 0);
    assert(E_PARSER_BUILD_CUSTOM == 1);
    assert(E_PARSER_BUILD_ENDOFKEY == 2);

    // resolve_parser_enum_time_t
    assert(E_PARSER_TIME_TAI == 0);
    assert(E_PARSER_TIME_ISO == 1);
    assert(E_PARSER_TIME_NONE == 2);
    assert(E_PARSER_TIME_ENDOFKEY == 3);

    // resolve_parser_enum_seed_t
    assert(E_PARSER_SEED_DEPENDS == 0);
    assert(E_PARSER_SEED_REQUIREDBY == 1);
    assert(E_PARSER_SEED_ENABLE == 2);
    assert(E_PARSER_SEED_ALLOW == 3);
    assert(E_PARSER_SEED_DENY == 4);
    assert(E_PARSER_SEED_CURRENT == 5);
    assert(E_PARSER_SEED_GROUPS == 6);
    assert(E_PARSER_SEED_CONTENTS == 7);
    assert(E_PARSER_SEED_ENDOFKEY == 8);

    // resolve_parser_enum_mandatory_t
    assert(E_PARSER_MANDATORY_NEED == 0);
    assert(E_PARSER_MANDATORY_OPTS == 1);
    assert(E_PARSER_MANDATORY_CUSTOM == 2);
    assert(E_PARSER_MANDATORY_ENDOFKEY == 3);

    // resolve_parser_enum_category_t
    assert(E_PARSER_CATEGORY_SECTION_MAIN == 0);
    assert(E_PARSER_CATEGORY_SECTION_STARTSTOP == 1);
    assert(E_PARSER_CATEGORY_SECTION_LOGGER == 2);
    assert(E_PARSER_CATEGORY_SECTION_ENVIRON == 3);
    assert(E_PARSER_CATEGORY_SECTION_REGEX == 4);
    assert(E_PARSER_CATEGORY_SECTION == 5);
    assert(E_PARSER_CATEGORY_IO_TYPE == 6);
    assert(E_PARSER_CATEGORY_TYPE == 7);
    assert(E_PARSER_CATEGORY_OPTS == 8);
    assert(E_PARSER_CATEGORY_FLAGS == 9);
    assert(E_PARSER_CATEGORY_BUILD == 10);
    assert(E_PARSER_CATEGORY_TIME == 11);
    assert(E_PARSER_CATEGORY_SEED == 12);
    assert(E_PARSER_CATEGORY_EXPECTED == 13);
    assert(E_PARSER_CATEGORY_MANDATORY == 14);
    assert(E_PARSER_CATEGORY_ENDOFKEY == 15);
}

// Test service enums (all values)
void test_service_enums(void) {
    printf("Testing service enums...\n");

    // resolve_service_enum_config_t
    assert(E_RESOLVE_SERVICE_CONFIG_RVERSION == 0);
    assert(E_RESOLVE_SERVICE_CONFIG_NAME == 1);
    assert(E_RESOLVE_SERVICE_CONFIG_DESCRIPTION == 2);
    assert(E_RESOLVE_SERVICE_CONFIG_VERSION == 3);
    assert(E_RESOLVE_SERVICE_CONFIG_TYPE == 4);
    assert(E_RESOLVE_SERVICE_CONFIG_NOTIFY == 5);
    assert(E_RESOLVE_SERVICE_CONFIG_MAXDEATH == 6);
    assert(E_RESOLVE_SERVICE_CONFIG_EARLIER == 7);
    assert(E_RESOLVE_SERVICE_CONFIG_COPYFROM == 8);
    assert(E_RESOLVE_SERVICE_CONFIG_INTREE == 9);
    assert(E_RESOLVE_SERVICE_CONFIG_OWNERSTR == 10);
    assert(E_RESOLVE_SERVICE_CONFIG_OWNER == 11);
    assert(E_RESOLVE_SERVICE_CONFIG_TREENAME == 12);
    assert(E_RESOLVE_SERVICE_CONFIG_USER == 13);
    assert(E_RESOLVE_SERVICE_CONFIG_INNS == 14);
    assert(E_RESOLVE_SERVICE_CONFIG_ENABLED == 15);
    assert(E_RESOLVE_SERVICE_CONFIG_ISLOG == 16);
    assert(E_RESOLVE_SERVICE_CONFIG_ENDOFKEY == 17);

    // resolve_service_enum_path_t
    assert(E_RESOLVE_SERVICE_PATH_HOME == 0);
    assert(E_RESOLVE_SERVICE_PATH_FRONTEND == 1);
    assert(E_RESOLVE_SERVICE_PATH_SERVICEDIR == 2);
    assert(E_RESOLVE_SERVICE_PATH_ENDOFKEY == 3);

    // resolve_service_enum_deps_t
    assert(E_RESOLVE_SERVICE_DEPS_DEPENDS == 0);
    assert(E_RESOLVE_SERVICE_DEPS_REQUIREDBY == 1);
    assert(E_RESOLVE_SERVICE_DEPS_OPTSDEPS == 2);
    assert(E_RESOLVE_SERVICE_DEPS_CONTENTS == 3);
    assert(E_RESOLVE_SERVICE_DEPS_NDEPENDS == 4);
    assert(E_RESOLVE_SERVICE_DEPS_NREQUIREDBY == 5);
    assert(E_RESOLVE_SERVICE_DEPS_NOPTSDEPS == 6);
    assert(E_RESOLVE_SERVICE_DEPS_NCONTENTS == 7);
    assert(E_RESOLVE_SERVICE_DEPS_ENDOFKEY == 8);

    // resolve_service_enum_execute_t
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN == 0);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_USER == 1);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_BUILD == 2);
    assert(E_RESOLVE_SERVICE_EXECUTE_RUN_RUNAS == 3);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH == 4);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_USER == 5);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_BUILD == 6);
    assert(E_RESOLVE_SERVICE_EXECUTE_FINISH_RUNAS == 7);
    assert(E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTART == 8);
    assert(E_RESOLVE_SERVICE_EXECUTE_TIMEOUTSTOP == 9);
    assert(E_RESOLVE_SERVICE_EXECUTE_DOWN == 10);
    assert(E_RESOLVE_SERVICE_EXECUTE_DOWNSIGNAL == 11);
    assert(E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY == 12);

    // resolve_service_enum_live_t
    assert(E_RESOLVE_SERVICE_LIVE_LIVEDIR == 0);
    assert(E_RESOLVE_SERVICE_LIVE_STATUS == 1);
    assert(E_RESOLVE_SERVICE_LIVE_SERVICEDIR == 2);
    assert(E_RESOLVE_SERVICE_LIVE_SCANDIR == 3);
    assert(E_RESOLVE_SERVICE_LIVE_STATEDIR == 4);
    assert(E_RESOLVE_SERVICE_LIVE_EVENTDIR == 5);
    assert(E_RESOLVE_SERVICE_LIVE_NOTIFDIR == 6);
    assert(E_RESOLVE_SERVICE_LIVE_SUPERVISEDIR == 7);
    assert(E_RESOLVE_SERVICE_LIVE_FDHOLDERDIR == 8);
    assert(E_RESOLVE_SERVICE_LIVE_ONESHOTDDIR == 9);
    assert(E_RESOLVE_SERVICE_LIVE_ENDOFKEY == 10);

    // resolve_service_enum_logger_t
    assert(E_RESOLVE_SERVICE_LOGGER_LOGNAME == 0);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGDESTINATION == 1);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGBACKUP == 2);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGMAXSIZE == 3);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGTIMESTAMP == 4);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGWANT == 5);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGRUN == 6);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGRUN_USER == 7);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGRUN_BUILD == 8);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGRUN_RUNAS == 9);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTART == 10);
    assert(E_RESOLVE_SERVICE_LOGGER_LOGTIMEOUTSTOP == 11);
    assert(E_RESOLVE_SERVICE_LOGGER_ENDOFKEY == 12);

    // resolve_service_enum_environ_t
    assert(E_RESOLVE_SERVICE_ENVIRON_ENV == 0);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENVDIR == 1);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENV_OVERWRITE == 2);
    assert(E_RESOLVE_SERVICE_ENVIRON_IMPORTFILE == 3);
    assert(E_RESOLVE_SERVICE_ENVIRON_NIMPORTFILE == 4);
    assert(E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY == 5);

    // resolve_service_enum_regex_t
    assert(E_RESOLVE_SERVICE_REGEX_CONFIGURE == 0);
    assert(E_RESOLVE_SERVICE_REGEX_DIRECTORIES == 1);
    assert(E_RESOLVE_SERVICE_REGEX_FILES == 2);
    assert(E_RESOLVE_SERVICE_REGEX_INFILES == 3);
    assert(E_RESOLVE_SERVICE_REGEX_NDIRECTORIES == 4);
    assert(E_RESOLVE_SERVICE_REGEX_NFILES == 5);
    assert(E_RESOLVE_SERVICE_REGEX_NINFILES == 6);
    assert(E_RESOLVE_SERVICE_REGEX_ENDOFKEY == 7);

    // resolve_service_enum_io_t
    assert(E_RESOLVE_SERVICE_IO_STDIN == 0);
    assert(E_RESOLVE_SERVICE_IO_STDINDEST == 1);
    assert(E_RESOLVE_SERVICE_IO_STDOUT == 2);
    assert(E_RESOLVE_SERVICE_IO_STDOUTDEST == 3);
    assert(E_RESOLVE_SERVICE_IO_STDERR == 4);
    assert(E_RESOLVE_SERVICE_IO_STDERRDEST == 5);
    assert(E_RESOLVE_SERVICE_IO_ENDOFKEY == 6);

    // resolve_service_enum_category_t
    assert(E_RESOLVE_SERVICE_CATEGORY_CONFIG == 0);
    assert(E_RESOLVE_SERVICE_CATEGORY_PATH == 1);
    assert(E_RESOLVE_SERVICE_CATEGORY_DEPS == 2);
    assert(E_RESOLVE_SERVICE_CATEGORY_EXECUTE == 3);
    assert(E_RESOLVE_SERVICE_CATEGORY_LIVE == 4);
    assert(E_RESOLVE_SERVICE_CATEGORY_LOGGER == 5);
    assert(E_RESOLVE_SERVICE_CATEGORY_ENVIRON == 6);
    assert(E_RESOLVE_SERVICE_CATEGORY_REGEX == 7);
    assert(E_RESOLVE_SERVICE_CATEGORY_IO == 8);
    assert(E_RESOLVE_SERVICE_CATEGORY_ENDOFKEY == 9);
}

// Test tree enums (all values)
void test_tree_enums(void) {
    printf("Testing tree enums...\n");

    // resolve_tree_enum_t
    assert(E_RESOLVE_TREE_RVERSION == 0);
    assert(E_RESOLVE_TREE_NAME == 1);
    assert(E_RESOLVE_TREE_ENABLED == 2);
    assert(E_RESOLVE_TREE_DEPENDS == 3);
    assert(E_RESOLVE_TREE_REQUIREDBY == 4);
    assert(E_RESOLVE_TREE_ALLOW == 5);
    assert(E_RESOLVE_TREE_GROUPS == 6);
    assert(E_RESOLVE_TREE_CONTENTS == 7);
    assert(E_RESOLVE_TREE_NDEPENDS == 8);
    assert(E_RESOLVE_TREE_NREQUIREDBY == 9);
    assert(E_RESOLVE_TREE_NALLOW == 10);
    assert(E_RESOLVE_TREE_NGROUPS == 11);
    assert(E_RESOLVE_TREE_NCONTENTS == 12);
    assert(E_RESOLVE_TREE_INIT == 13);
    assert(E_RESOLVE_TREE_SUPERVISED == 14);
    assert(E_RESOLVE_TREE_ENDOFKEY == 15);

    // resolve_tree_master_enum_t
    assert(E_RESOLVE_TREE_MASTER_RVERSION == 0);
    assert(E_RESOLVE_TREE_MASTER_NAME == 1);
    assert(E_RESOLVE_TREE_MASTER_ALLOW == 2);
    assert(E_RESOLVE_TREE_MASTER_CURRENT == 3);
    assert(E_RESOLVE_TREE_MASTER_CONTENTS == 4);
    assert(E_RESOLVE_TREE_MASTER_NALLOW == 5);
    assert(E_RESOLVE_TREE_MASTER_NCONTENTS == 6);
    assert(E_RESOLVE_TREE_MASTER_ENDOFKEY == 7);

    // resolve_tree_enum_category_t
    assert(E_RESOLVE_TREE_CATEGORY_TREE == 0);
    assert(E_RESOLVE_TREE_CATEGORY_MASTER == 1);
    assert(E_RESOLVE_TREE_CATEGORY_ENDOFKEY == 2);

    // resolve_enum_category_t
    assert(E_RESOLVE_CATEGORY_PARSER == 0);
    assert(E_RESOLVE_CATEGORY_SERVICE == 1);
    assert(E_RESOLVE_CATEGORY_TREE == 2);
    assert(E_RESOLVE_CATEGORY_ENDOFKEY == 3);
}

// Test parser lists (all entries)
void test_parser_lists(void) {
    printf("Testing parser lists...\n");

    // enum_list_parser_expected
    assert(count_key_description_entries(enum_list_parser_expected) == 6);
    assert(count_string_array_entries(enum_str_parser_expected) == 6);
    for (size_t i = 0; i < 6; i++) {
        assert(enum_list_parser_expected[i].id == (int)i);
        assert(strcmp(*enum_list_parser_expected[i].name, enum_str_parser_expected[i]) == 0);
        assert(enum_list_parser_expected[i].expected == 0);
    }

    // enum_list_parser_section
    assert(count_key_description_entries(enum_list_parser_section) == 6);
    assert(count_string_array_entries(enum_str_parser_section) == 6);
    for (size_t i = 0; i < 6; i++) {
        assert(enum_list_parser_section[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section[i].name, enum_str_parser_section[i]) == 0);
        assert(enum_list_parser_section[i].expected == 0);
    }

    // enum_list_parser_section_main
    assert(count_key_description_entries(enum_list_parser_section_main) == 20);
    assert(count_string_array_entries(enum_str_parser_section_main) == 20);
    for (size_t i = 0; i < 20; i++) {
        assert(enum_list_parser_section_main[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_main[i].name, enum_str_parser_section_main[i]) == 0);
        // Check expected values (from SECTION_MAIN_TEMPLATE)
        int expected_values[] = {
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_QUOTE, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_BRACKET,
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE
        };
        assert(enum_list_parser_section_main[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_startstop
    assert(count_key_description_entries(enum_list_parser_section_startstop) == 3);
    assert(count_string_array_entries(enum_str_parser_section_startstop) == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_section_startstop[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_startstop[i].name, enum_str_parser_section_startstop[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_BRACKET };
        assert(enum_list_parser_section_startstop[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_logger
    assert(count_key_description_entries(enum_list_parser_section_logger) == 9);
    assert(count_string_array_entries(enum_str_parser_section_logger) == 9);
    for (size_t i = 0; i < 9; i++) {
        assert(enum_list_parser_section_logger[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_logger[i].name, enum_str_parser_section_logger[i]) == 0);
        int expected_values[] = {
            E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_SLASH,
            E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_UINT, E_PARSER_EXPECT_LINE, E_PARSER_EXPECT_UINT,
            E_PARSER_EXPECT_UINT
        };
        assert(enum_list_parser_section_logger[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_environ
    assert(count_key_description_entries(enum_list_parser_section_environ) == 2);
    assert(count_string_array_entries(enum_str_parser_section_environ) == 2);
    for (size_t i = 0; i < 2; i++) {
        assert(enum_list_parser_section_environ[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_environ[i].name, enum_str_parser_section_environ[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_KEYVAL, E_PARSER_EXPECT_BRACKET };
        assert(enum_list_parser_section_environ[i].expected == expected_values[i]);
    }

    // enum_list_parser_section_regex
    assert(count_key_description_entries(enum_list_parser_section_regex) == 4);
    assert(count_string_array_entries(enum_str_parser_section_regex) == 4);
    for (size_t i = 0; i < 4; i++) {
        assert(enum_list_parser_section_regex[i].id == (int)i);
        assert(strcmp(*enum_list_parser_section_regex[i].name, enum_str_parser_section_regex[i]) == 0);
        int expected_values[] = { E_PARSER_EXPECT_QUOTE, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET, E_PARSER_EXPECT_BRACKET };
        assert(enum_list_parser_section_regex[i].expected == expected_values[i]);
    }

    // enum_list_parser_io_type
    assert(count_key_description_entries(enum_list_parser_io_type) == 10);
    assert(count_string_array_entries(enum_str_parser_io_type) == 10);
    for (size_t i = 0; i < 10; i++) {
        assert(enum_list_parser_io_type[i].id == (int)i);
        assert(strcmp(*enum_list_parser_io_type[i].name, enum_str_parser_io_type[i]) == 0);
        assert(enum_list_parser_io_type[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_type
    assert(count_key_description_entries(enum_list_parser_type) == 3);
    assert(count_string_array_entries(enum_str_parser_type) == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_type[i].id == (int)i);
        assert(strcmp(*enum_list_parser_type[i].name, enum_str_parser_type[i]) == 0);
        assert(enum_list_parser_type[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_opts
    assert(count_key_description_entries(enum_list_parser_opts) == 1);
    assert(count_string_array_entries(enum_str_parser_opts) == 1);
    assert(enum_list_parser_opts[0].id == 0);
    assert(strcmp(*enum_list_parser_opts[0].name, enum_str_parser_opts[0]) == 0);
    assert(enum_list_parser_opts[0].expected == E_PARSER_EXPECT_BRACKET);

    // enum_list_parser_flags
    assert(count_key_description_entries(enum_list_parser_flags) == 2);
    assert(count_string_array_entries(enum_str_parser_flags) == 2);
    for (size_t i = 0; i < 2; i++) {
        assert(enum_list_parser_flags[i].id == (int)i);
        assert(strcmp(*enum_list_parser_flags[i].name, enum_str_parser_flags[i]) == 0);
        assert(enum_list_parser_flags[i].expected == E_PARSER_EXPECT_BRACKET);
    }

    // enum_list_parser_build
    assert(count_key_description_entries(enum_list_parser_build) == 2);
    assert(count_string_array_entries(enum_str_parser_build) == 2);
    for (size_t i = 0; i < 2; i++) {
        assert(enum_list_parser_build[i].id == (int)i);
        assert(strcmp(*enum_list_parser_build[i].name, enum_str_parser_build[i]) == 0);
        assert(enum_list_parser_build[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_time
    assert(count_key_description_entries(enum_list_parser_time) == 3);
    assert(count_string_array_entries(enum_str_parser_time) == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_parser_time[i].id == (int)i);
        assert(strcmp(*enum_list_parser_time[i].name, enum_str_parser_time[i]) == 0);
        assert(enum_list_parser_time[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_seed
    assert(count_key_description_entries(enum_list_parser_seed) == 8);
    assert(count_string_array_entries(enum_str_parser_seed) == 8);
    for (size_t i = 0; i < 8; i++) {
        assert(enum_list_parser_seed[i].id == (int)i);
        assert(strcmp(*enum_list_parser_seed[i].name, enum_str_parser_seed[i]) == 0);
        assert(enum_list_parser_seed[i].expected == E_PARSER_EXPECT_LINE);
    }

    // enum_list_parser_mandatory
    assert(count_key_description_entries(enum_list_parser_mandatory) == 3);
    assert(count_string_array_entries(enum_str_parser_mandatory) == 3);
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
    assert(count_key_description_entries(enum_list_service_config) == 17);
    assert(count_string_array_entries(enum_str_service_config) == 17);
    for (size_t i = 0; i < 17; i++) {
        assert(enum_list_service_config[i].id == (int)i);
        assert(strcmp(*enum_list_service_config[i].name, enum_str_service_config[i]) == 0);
        assert(enum_list_service_config[i].expected == 0); // No expected field defined
    }

    // enum_list_service_path
    assert(count_key_description_entries(enum_list_service_path) == 3);
    assert(count_string_array_entries(enum_str_service_path) == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(enum_list_service_path[i].id == (int)i);
        assert(strcmp(*enum_list_service_path[i].name, enum_str_service_path[i]) == 0);
        assert(enum_list_service_path[i].expected == 0);
    }

    // enum_list_service_deps
    assert(count_key_description_entries(enum_list_service_deps) == 8);
    assert(count_string_array_entries(enum_str_service_deps) == 8);
    for (size_t i = 0; i < 8; i++) {
        assert(enum_list_service_deps[i].id == (int)i);
        assert(strcmp(*enum_list_service_deps[i].name, enum_str_service_deps[i]) == 0);
        assert(enum_list_service_deps[i].expected == 0);
    }

    // enum_list_service_execute
    assert(count_key_description_entries(enum_list_service_execute) == 12);
    assert(count_string_array_entries(enum_str_service_execute) == 12);
    for (size_t i = 0; i < 12; i++) {
        assert(enum_list_service_execute[i].id == (int)i);
        assert(strcmp(*enum_list_service_execute[i].name, enum_str_service_execute[i]) == 0);
        assert(enum_list_service_execute[i].expected == 0);
    }

    // enum_list_service_live
    assert(count_key_description_entries(enum_list_service_live) == 10);
    assert(count_string_array_entries(enum_str_service_live) == 10);
    for (size_t i = 0; i < 10; i++) {
        assert(enum_list_service_live[i].id == (int)i);
        assert(strcmp(*enum_list_service_live[i].name, enum_str_service_live[i]) == 0);
        assert(enum_list_service_live[i].expected == 0);
    }

    // enum_list_service_logger
    assert(count_key_description_entries(enum_list_service_logger) == 12);
    assert(count_string_array_entries(enum_str_service_logger) == 12);
    for (size_t i = 0; i < 12; i++) {
        assert(enum_list_service_logger[i].id == (int)i);
        assert(strcmp(*enum_list_service_logger[i].name, enum_str_service_logger[i]) == 0);
        assert(enum_list_service_logger[i].expected == 0);
    }

    // enum_list_service_environ
    assert(count_key_description_entries(enum_list_service_environ) == 5);
    assert(count_string_array_entries(enum_str_service_environ) == 5);
    for (size_t i = 0; i < 5; i++) {
        assert(enum_list_service_environ[i].id == (int)i);
        assert(strcmp(*enum_list_service_environ[i].name, enum_str_service_environ[i]) == 0);
        assert(enum_list_service_environ[i].expected == 0);
    }

    // enum_list_service_regex
    assert(count_key_description_entries(enum_list_service_regex) == 7);
    assert(count_string_array_entries(enum_str_service_regex) == 7);
    for (size_t i = 0; i < 7; i++) {
        assert(enum_list_service_regex[i].id == (int)i);
        assert(strcmp(*enum_list_service_regex[i].name, enum_str_service_regex[i]) == 0);
        assert(enum_list_service_regex[i].expected == 0);
    }

    // enum_list_service_io
    assert(count_key_description_entries(enum_list_service_io) == 6);
    assert(count_string_array_entries(enum_str_service_io) == 6);
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
    assert(count_key_description_entries(enum_list_tree) == 15);
    assert(count_string_array_entries(enum_str_tree) == 15);
    for (size_t i = 0; i < 15; i++) {
        assert(enum_list_tree[i].id == (int)i);
        assert(strcmp(*enum_list_tree[i].name, enum_str_tree[i]) == 0);
        assert(enum_list_tree[i].expected == 0);
    }

    // enum_list_tree_master
    assert(count_key_description_entries(enum_list_tree_master) == 7);
    assert(count_string_array_entries(enum_str_tree_master) == 7);
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