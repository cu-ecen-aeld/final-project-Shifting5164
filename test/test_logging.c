#include <stddef.h>
#include <stdarg.h>
#include <cmocka.h>
#include "../src/logger.c"

static void logger_no_file(void **state) {
    assert_false(logger_init("/"));
    assert_false(logger_destroy());
}

static void logger_error_entry(void **state) {
    assert_false(logger_init("/var/log/testlog"));

    assert_false(log_error("my error is %d", 42));

    assert_false(logger_destroy());
}

const struct CMUnitTest test_logging[] = {
        cmocka_unit_test(logger_no_file),
        cmocka_unit_test(logger_error_entry),
};
