#include <stddef.h>
#include <cmocka.h>
#include "../src/settings.c"

static void settings_no_file(void **state) {
    assert_true(settings_load("none"));
}

/* read invalid file, so entry should be ignored, and options should be '0' */
static void settings_wrong_file(void **state) {
    sSettingsStruct *settings = settings_init();

    assert_true(settings_load("/dev/null"));

    assert_int_equal(settings->lMaxClientsPerThread, 0);
    assert_int_equal(settings->lWorkerThreads, 0);
}

/* read basic information, all valid*/
static void settings_basic_valid_file(void **state) {

    sSettingsStruct *settings = settings_init();

    assert_false(settings_load("ini/valid_settings.ini"));

    assert_int_equal(settings->lMaxClientsPerThread, 50);
    assert_int_equal(settings->lWorkerThreads, 4);
}

/* read invalid settings, so entry should be ignored, and options should be '0' */
static void settings_basic_invalid_file(void **state) {
    sSettingsStruct *settings = settings_init();

    assert_false(settings_load("ini/invalid_settings.ini"));

    assert_int_equal(settings->lMaxClientsPerThread, 0);
    assert_int_equal(settings->lWorkerThreads, 0);
}

const struct CMUnitTest test_settings[] = {
        cmocka_unit_test(settings_no_file),
        cmocka_unit_test(settings_wrong_file),
        cmocka_unit_test(settings_basic_valid_file),
        cmocka_unit_test(settings_basic_invalid_file),
};
