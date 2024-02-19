#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/cew_worker.c"

//happy flow
static void worker_happy_init(void **state) {
    assert_false(worker_init(1));
    assert_false(worker_destroy());

//    worker_test(5);
}

const struct CMUnitTest test_worker[] = {
        cmocka_unit_test(worker_happy_init),
};
