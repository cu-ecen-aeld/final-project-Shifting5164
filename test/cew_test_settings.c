#include <stddef.h>
#include <cmocka.h>
#include "../src/cew_settings.c"

static void settings_no_file(void **state) {
    assert_true(settings_load("none"));
}

/* read invalid file, so entry should be ignored, and options should be '0' */
static void settings_wrong_file(void **state) {
    assert_true(settings_load("/dev/null"));

    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 0);
    assert_int_equal(gsCurrSSettings.lWorkerThreads, 0);

    settings_destroy();
}

/* read basic information, all valid*/
static void settings_basic_valid_file(void **state) {

    assert_false(settings_load("ini/valid_settings.ini"));

    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 50);
    assert_int_equal(gsCurrSSettings.lWorkerThreads, 4);

    settings_destroy();
}

/* read invalid settings, so entry should be ignored, and options should be '0' */
static void settings_basic_invalid_file(void **state) {

    assert_false(settings_load("ini/invalid_settings.ini"));

    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 0);
    assert_int_equal(gsCurrSSettings.lWorkerThreads, 0);

    settings_destroy();
}

static void settings_dual_settings(void **state) {

    assert_false(settings_load("ini/valid_dual_settings.ini"));

    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 51);
    assert_int_equal(gsCurrSSettings.lWorkerThreads, 4);
    assert_non_null(gsCurrSSettings.pcLogfile);
    assert_string_equal(gsCurrSSettings.pcLogfile, "/var/log/cewserver2.log" );

    settings_destroy();
}

static void settings_get_and_set_settings(void **state) {

    assert_false(settings_load("ini/valid_dual_settings.ini"));

    // read if valid
    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 51);
    assert_int_equal(gsCurrSSettings.lWorkerThreads, 4);
    assert_non_null(gsCurrSSettings.pcLogfile);
    assert_string_equal(gsCurrSSettings.pcLogfile, "/var/log/cewserver2.log" );

    // test if modifying is only local (get a copy)
    tsSSettings old = settings_get();
    old.lMaxClientsPerThread = 55;
    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 51);

    // set settings, and read back
    settings_set(old);
    assert_int_equal(gsCurrSSettings.lMaxClientsPerThread, 55);
    old = settings_get();
    assert_int_equal(old.lMaxClientsPerThread, 55);

    settings_destroy();

}


const struct CMUnitTest test_settings[] = {
        cmocka_unit_test(settings_no_file),
        cmocka_unit_test(settings_wrong_file),
        cmocka_unit_test(settings_basic_valid_file),
        cmocka_unit_test(settings_basic_invalid_file),
        cmocka_unit_test(settings_dual_settings),
        cmocka_unit_test(settings_get_and_set_settings),
};
