#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/cew_worker.c"
#include "../include/cew_logger.h"
#include "../include/cew_client.h"

//happy flow, memcheck
static void worker_happy_init(void **state) {
    assert_false(worker_init(1));
    assert_false(worker_destroy());
}

//happy flow, add some clients, memcheck
static void worker_happy_client_add(void **state) {
    unlink(testfile);

    assert_false(logger_init("/var/tmp/testlog_client_add", eDEBUG));
    assert_false(worker_init(10));

    for (int i = 0; i < 17; i++) {
        tsClientStruct *psAddClient = malloc(sizeof(struct sClientStruct));
        psAddClient->iId = i;
        assert_false(worker_route_client(psAddClient));
    }

    assert_false(worker_destroy());
    assert_false(logger_destroy());
}


const struct CMUnitTest test_worker[] = {
        cmocka_unit_test(worker_happy_init),
        cmocka_unit_test(worker_happy_client_add),
};
