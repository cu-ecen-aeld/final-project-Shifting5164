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
    unlink(testfile);

    assert_false(logger_init(testfile, eDEBUG));
    assert_true(logger_init(testfile, eDEBUG));
    assert_false(logger_destroy());

    unlink(testfile);
}

//may not dual destroy
static void logger_dual_destroy(void **state) {
    unlink(testfile);

    assert_false(logger_init(testfile, eDEBUG));
    assert_false(logger_destroy());
    assert_true(logger_destroy());

    unlink(testfile);
}

// normal usage
static void logger_error_entry(void **state) {
    unlink(testfile);

    assert_false(logger_init(testfile, eDEBUG));
    assert_false(log_error("my error is %d", 42));
    assert_false(logger_flush());
    assert_false(logger_destroy());

    unlink(testfile);
}

// log message without init
static void logger_no_init(void **state) {
    assert_true(log_error("my error is %d", 42));
}

static void logger_check_levels(void **state) {
    unlink(testfile);

    assert_false(logger_init(testfile, eDEBUG));

    //---------------------------
    {
        assert_false(log_error("my error is %d", 42));

        FILE *fd = fopen(testfile, "r");
        char filedata[100] = {0};
        char testmsg[] = "Error : my error is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    //---------------------------
    {
        assert_false(log_error("my warning is %d", 42));

        FILE *fd = fopen(testfile, "r");
        char filedata[100] = {0};
        char testmsg[] = "Warning : my warning is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    //---------------------------
    {
        assert_false(log_error("my info is %d", 42));

        FILE *fd = fopen(testfile, "r");
        char filedata[100] = {0};
        char testmsg[] = "Info : my info is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    //---------------------------
    {
        assert_false(log_error("my debug is %d", 42));

        FILE *fd = fopen(testfile, "r");
        char filedata[100] = {0};
        char testmsg[] = "Debug : my debug is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    assert_false(logger_destroy());
    unlink(testfile);
}

const struct CMUnitTest test_logging[] = {
        cmocka_unit_test(logger_no_file),
        cmocka_unit_test(logger_dual_init),
        cmocka_unit_test(logger_dual_destroy),
        cmocka_unit_test(logger_error_entry),
        cmocka_unit_test(logger_no_init),
        cmocka_unit_test(logger_check_levels),

};
