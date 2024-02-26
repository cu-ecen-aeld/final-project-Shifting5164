#include <stddef.h>
#include <stdarg.h>
#include <cmocka.h>
#include "../src/cew_logger.c"

static char logging_testfile[] = "/var/tmp/testlog";

//no acess to file, should fail
static void logger_no_file(void **state) {
    assert_true(logger_init("/a/path/that/doesnt/exists", eDEBUG));
    logger_destroy();
}

//may not dual init, but should destroy correctly
static void logger_dual_init(void **state) {
    unlink(logging_testfile);

    assert_false(logger_init(logging_testfile, eDEBUG));
    assert_true(logger_init(logging_testfile, eDEBUG));
    assert_false(logger_destroy());

    unlink(logging_testfile);
}

//may not dual destroy
static void logger_dual_destroy(void **state) {
    unlink(logging_testfile);

    assert_false(logger_init(logging_testfile, eDEBUG));
    assert_false(logger_destroy());
    assert_true(logger_destroy());

    unlink(logging_testfile);
}

// normal usage
static void logger_error_entry(void **state) {
    unlink(logging_testfile);

    assert_false(logger_init(logging_testfile, eDEBUG));
    assert_false(log_error("my error is %d", 42));
    assert_false(logger_flush());
    assert_false(logger_destroy());

    unlink(logging_testfile);
}

// log message without init
static void logger_no_init(void **state) {
    assert_true(log_error("my error is %d", 42));
}

// no init but do destroy
static void logger_no_init_and_destroy(void **state) {
    assert_true(logger_destroy());
}

// see if things get written on the correct levels
static void logger_check_levels(void **state) {


    //---------------------------
    {
        unlink(logging_testfile);
        assert_false(logger_init(logging_testfile, eDEBUG));
        assert_false(log_error("my error is %d", 42));
        logger_flush();

        FILE *fd = fopen(logging_testfile, "r");
        char filedata[1024] = {0};
        char testmsg[] = "Error : my error is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_non_null(strstr(filedata, testmsg));
        assert_false(logger_destroy());
    }

    //---------------------------
    {
        unlink(logging_testfile);
        assert_false(logger_init(logging_testfile, eDEBUG));
        assert_false(log_error("my warning is %d", 42));
        logger_flush();

        FILE *fd = fopen(logging_testfile, "r");
        char filedata[1024] = {0};
        char testmsg[] = "Error : my warning is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_non_null(strstr(filedata, testmsg));
        assert_false(logger_destroy());
    }

    //---------------------------
    {
        unlink(logging_testfile);
        assert_false(logger_init(logging_testfile, eDEBUG));
        assert_false(log_error("my info is %d", 42));

        FILE *fd = fopen(logging_testfile, "r");
        char filedata[1024] = {0};
        char testmsg[] = "Info : my info is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    //---------------------------
    {
        assert_false(log_error("my debug is %d", 42));

        FILE *fd = fopen(logging_testfile, "r");
        char filedata[100] = {0};
        char testmsg[] = "Debug : my debug is 42";
        fread(&filedata, sizeof(filedata), 1, fd);
        fclose(fd);

        assert_null(strstr(filedata, testmsg));
    }

    assert_false(logger_destroy());
    unlink(logging_testfile);
}

// see what get accepted, local and global
static void logger_get_and_set_settings(void **state) {

    assert_false(logger_init(logging_testfile, eDEBUG));

    // test if modifying is only local (get a copy)
    tsLogSettings old = logger_get();
    old.iCurrLogLevel = 5;
    assert_int_equal(sCurrLogSettings.iCurrLogLevel, 3);

    // set settings, and read back
    logger_set(old);
    assert_int_equal(sCurrLogSettings.iCurrLogLevel, 5);
    old = logger_get();
    assert_int_equal(old.iCurrLogLevel, 5);

    logger_destroy();
}

// test all loglevels, if they are accepted or not
static void logger_check_level_filter(void **state) {
    unlink(logging_testfile);

    assert_false(logger_init(logging_testfile, eERROR));
    assert_false(log_error("my error is %d", 42));
    assert_true(log_warning("my warning is %d", 42));
    assert_true(log_info("my info is %d", 42));
    assert_true(log_debug("my debug is %d", 42));

    sCurrLogSettings.iCurrLogLevel = eWARNING;
    assert_false(log_error("my error is %d", 42));
    assert_false(log_warning("my warning is %d", 42));
    assert_true(log_info("my info is %d", 42));
    assert_true(log_debug("my debug is %d", 42));

    sCurrLogSettings.iCurrLogLevel = eINFO;
    assert_false(log_error("my error is %d", 42));
    assert_false(log_warning("my warning is %d", 42));
    assert_false(log_info("my info is %d", 42));
    assert_true(log_debug("my debug is %d", 42));

    sCurrLogSettings.iCurrLogLevel = eDEBUG;
    assert_false(log_error("my error is %d", 42));
    assert_false(log_warning("my warning is %d", 42));
    assert_false(log_info("my info is %d", 42));
    assert_false(log_debug("my debug is %d", 42));

    assert_false(logger_destroy());
    unlink(logging_testfile);

}

static void *write_log(void *arg) {
    int32_t count = 100;
    char *id = (char *) arg;

    int32_t curr_count = 0;
    do {
        log_error("my error is %d, from t:%s, c:%d", 42, id, curr_count);
        usleep(5);
        log_warning("my warning is %d, from t:%s, c:%d", 42, id, curr_count);
        usleep(11);
        log_info("my info is %d, from t:%s, c:%d", 42, id, curr_count);
        usleep(7);
        log_debug("my debug is %d, from t:%s, c:%d", 42, id, curr_count);
        usleep(14);
        curr_count++;
    } while (count--);

    return(0);
}

// test multithreading of this design
static void logger_check_mutithreading(void **state) {
    unlink(logging_testfile);
    assert_false(logger_init(logging_testfile, eDEBUG));

    int32_t test_threads = 10;
    struct sThreads {
        pthread_t th;
        char id[5];
    };

    struct sThreads threads[test_threads];

    for (int32_t i = 0; i < test_threads; i++) {
        memset(threads[i].id, 0, sizeof(threads[i].id));
        snprintf(threads[i].id, sizeof(threads[i].id), "%d", i);
        pthread_create(&threads[i].th, NULL, write_log, &threads[i].id);
    }

    for (int32_t i = 0; i < test_threads; i++) {
        pthread_join(threads[i].th, NULL);
    }

    logger_destroy();
}

const struct CMUnitTest test_logging[] = {
        cmocka_unit_test(logger_no_file),
        cmocka_unit_test(logger_dual_init),
        cmocka_unit_test(logger_dual_destroy),
        cmocka_unit_test(logger_error_entry),
        cmocka_unit_test(logger_no_init),
        cmocka_unit_test(logger_check_levels),
        cmocka_unit_test(logger_no_init_and_destroy),
        cmocka_unit_test(logger_get_and_set_settings),
        cmocka_unit_test(logger_check_level_filter),
        cmocka_unit_test(logger_check_mutithreading),
};
