#include <stddef.h>
#include <cmocka.h>
#include "../src/cew_socket.c"

//happy flow
static void socket_happy_connect(void **state) {
    assert_false(socket_setup("2001"));
    assert_false(socket_close());
}

const struct CMUnitTest test_socket[] = {
        cmocka_unit_test(socket_happy_connect),
};
