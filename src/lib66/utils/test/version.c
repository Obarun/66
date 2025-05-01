#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <66/utils.h>

#define ASSERT_EQ(actual, expected, msg) do { \
    int _actual = (actual); \
    int _expected = (expected); \
    if (_actual != _expected) { \
        printf("Assertion failed: %s (got %d, expected %d)\n", msg, _actual, _expected); \
        assert(0); \
    } \
} while (0)

#define ASSERT_STR_EQ(actual, expected, msg) do { \
    const char *_actual = (actual); \
    const char *_expected = (expected); \
    if (strcmp(_actual, _expected) != 0) { \
        printf("Assertion failed: %s (got %s, expected %s)\n", msg, _actual, _expected); \
        assert(0); \
    } \
} while (0)

void test_version_compare() {
    printf("Running test_version_compare\n") ;

    // Equal versions
    ASSERT_EQ(version_compare("1.0.0", "1.0.0"), 0, "1.0.0 == 1.0.0");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ab.01-1"), 0, "complex version equal");

    // Numeric comparisons
    ASSERT_EQ(version_compare("1.0.0", "1.0.1"), -1, "1.0.0 < 1.0.1");
    ASSERT_EQ(version_compare("1.0.1", "1.0.0"), 1, "1.0.1 > 1.0.0");
    ASSERT_EQ(version_compare("2.0.0", "1.0.0"), 1, "2.0.0 > 1.0.0");
    ASSERT_EQ(version_compare("1.0.0", "2.0.0"), -1, "1.0.0 < 2.0.0");

    // Pre-release versions
    ASSERT_EQ(version_compare("1.0.0-alpha", "1.0.0"), -1, "1.0.0-alpha < 1.0.0");
    ASSERT_EQ(version_compare("1.0.0", "1.0.0-alpha"), 1, "1.0.0 > 1.0.0-alpha");
    ASSERT_EQ(version_compare("1.0.0-alpha", "1.0.0-beta"), -1, "1.0.0-alpha < 1.0.0-beta");
    ASSERT_EQ(version_compare("1.0.0-beta", "1.0.0-alpha"), 1, "1.0.0-beta > 1.0.0-alpha");

    // Mixed components
    ASSERT_EQ(version_compare("1.0.0-123abc", "1.0.0-123abc"), 0, "1.0.0-123abc == 1.0.0-123abc");
    ASSERT_EQ(version_compare("1.0.0-123abc", "1.0.0-123def"), -1, "1.0.0-123abc < 1.0.0-123def");
    ASSERT_EQ(version_compare("1.0.0-123def", "1.0.0-123abc"), 1, "1.0.0-123def > 1.0.0-123abc");
    ASSERT_EQ(version_compare("1.0.0-123abc", "1.0.0-124abc"), -1, "1.0.0-123abc < 1.0.0-124abc");
    ASSERT_EQ(version_compare("1.0.0-123abc", "1.0.0"), 1, "1.0.0-123abc > 1.0.0");
    ASSERT_EQ(version_compare("1.0.0", "1.0.0-123abc"), -1, "1.0.0 < 1.0.0-123abc");

    // Letter suffix
    ASSERT_EQ(version_compare("1.0.0-alpha.1", "1.0.0-alpha.2"), -1, "1.0.0-alpha.1 < 1.0.0-alpha.2");
    ASSERT_EQ(version_compare("1.0.0-alpha.2", "1.0.0-alpha.1"), 1, "1.0.0-alpha.2 > 1.0.0-alpha.1");

    // Different lengths
    ASSERT_EQ(version_compare("1.0", "1.0.0"), 0, "1.0 == 1.0.0");
    ASSERT_EQ(version_compare("1.0.0", "1.0"), 0, "1.0.0 == 1.0");
    ASSERT_EQ(version_compare("1.0.0.0", "1.0.0"), 0, "1.0.0.0 == 1.0.0");

    // Leading zeros
    ASSERT_EQ(version_compare("01.00.00", "1.0.0"), 0, "01.00.00 == 1.0.0");
    ASSERT_EQ(version_compare("1.0.0", "001.000.000"), 0, "1.0.0 == 001.000.000");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ab.001-1"), 0, "1.0ab.01-1 == 1.0ab.001-1");

    // Separators
    ASSERT_EQ(version_compare("1-0-0", "1.0.0"), 0, "1-0-0 == 1.0.0");
    ASSERT_EQ(version_compare("1..0--0", "1.0.0"), 0, "1..0--0 == 1.0.0");
    ASSERT_EQ(version_compare("1-0ab-01--1", "1.0ab.01-1"), 0, "1-0ab-01--1 == 1.0ab.01-1");

    // Complex cases
    ASSERT_EQ(version_compare("1.0.0-alpha.1", "1.0.0-alpha.2"), -1, "1.0.0-alpha.1 < 1.0.0-alpha.2");
    ASSERT_EQ(version_compare("1.0.0-alpha", "1.0.0-alpha.1"), -1, "1.0.0-alpha < 1.0.0-alpha.1");
    ASSERT_EQ(version_compare("1.0.0-alpha.1", "1.0.0"), -1, "1.0.0-alpha.1 < 1.0.0");

    // Test version comparisons
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ab.01-1"), 0, "1.0ab.01-1 == 1.0ab.01-1");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ab.01-2"), -1, "1.0ab.01-1 < 1.0ab.01-2");
    ASSERT_EQ(version_compare("1.0ab.01-2", "1.0ab.01-1"), 1, "1.0ab.01-2 > 1.0ab.01-1");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ab.02-1"), -1, "1.0ab.01-1 < 1.0ab.02-1");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0ac.01-1"), 0, "1.0ab.01-1 == 1.0ac.01-1");
    ASSERT_EQ(version_compare("1.0ab.01-1", "1.0.01-1"), 1, "1.0ab.01-1 > 1.0.01-1");
    ASSERT_EQ(version_compare("1.0.01-1", "1.0ab.01-1"), -1, "1.0.01-1 < 1.0ab.01-1");
}

int main() {
    printf("Starting tests...\n");
    test_version_compare();
    printf("All tests passed!\n");
    return 0;
}