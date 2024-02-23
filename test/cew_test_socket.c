#include <stddef.h>
#include <cmocka.h>
#include "../src/cew_socket.c"

//happy flow
static void socket_happy_connect(void **state) {
    assert_false(socket_setup(2001));
    assert_false(socket_close());
}

// happy flow 2 times, check mem leaks, and socket in use
// should not throw "Address already in use"
static void socket_happy_connect2(void **state) {
    assert_false(socket_setup(5001));
    assert_false(socket_close());

    assert_false(socket_setup(5001));
    assert_false(socket_close());
}

//accept connection, and disconnect
static void socket_connect(void **state) {
    //TODO
}

const struct CMUnitTest test_socket[] = {
        cmocka_unit_test(socket_happy_connect),
        cmocka_unit_test(socket_happy_connect2),
        cmocka_unit_test(socket_connect),
};
