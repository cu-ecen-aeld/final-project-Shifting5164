#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/cew_worker.c"
#include "../src/cew_client.c"
#include "../include/cew_logger.h"
#include "../include/cew_client.h"

//happy flow, memcheck
static void worker_happy_init(void **state) {

    static char worker_testfile[] = "/var/tmp/worker_testlog";

    unlink(worker_testfile);

    assert_false(logger_init(worker_testfile, eDEBUG));
    assert_false(worker_init(1));

    assert_false(worker_destroy());
    assert_false(logger_destroy());

}

const struct CMUnitTest test_worker[] = {
        cmocka_unit_test(worker_happy_init),
};
