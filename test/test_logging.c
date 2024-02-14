#include <stddef.h>
#include <stdarg.h>
#include <cmocka.h>
#include "../src/logger.c"

static char testfile[] = "/var/log/testlog";

//no acess to file, should fail
static void logger_no_file(void **state) {
    assert_true(logger_init("/a/path/that/doesnt/exists", eDEBUG));
    logger_destroy();
}

//may not dual init, but should destroy correctly
static void logger_dual_init(void **state) {
    assert_false(logger_init(testfile, eDEBUG));
    assert_true(logger_init(testfile, eDEBUG));
    assert_false(logger_destroy());

    unlink(testfile);
}

//may not dual destroy
static void logger_dual_destroy(void **state) {
    assert_false(logger_init(testfile, eDEBUG));
    assert_false(logger_destroy());
    assert_true(logger_destroy());

    unlink(testfile);
}

static void logger_error_entry(void **state) {
    assert_false(logger_init(testfile, eDEBUG));
    assert_false(log_error("my error is %d", 42));
    logger_flush();
    unlink(testfile);
}

const struct CMUnitTest test_logging[] = {
        cmocka_unit_test(logger_no_file),
        cmocka_unit_test(logger_dual_init),
        cmocka_unit_test(logger_dual_destroy),
        cmocka_unit_test(logger_error_entry),
};
