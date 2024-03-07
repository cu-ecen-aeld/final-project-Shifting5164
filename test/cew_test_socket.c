#include <stddef.h>
#include <cmocka.h>
#include <curl/curl.h>
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

//try a connection for something that doesn't exists yet
static void socket_try_connect(void **state) {
    CURL *curl = curl_easy_init();
    assert_non_null(curl);
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5001");
    curl_easy_setopt(curl, CURLOPT_SERVER_RESPONSE_TIMEOUT, 1L);   //1 sec timeout for connect
    assert_int_not_equal(curl_easy_perform(curl), 0);
    curl_easy_cleanup(curl);
    curl_global_cleanup();  // for valgrind
}

static void test_socket_listen_cleanup(void *arg) {
    assert_false(socket_close());
    assert_false(worker_destroy());
}

static void *test_socket_listen(void *arg) {

    int oldtype;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);

    pthread_cleanup_push(test_socket_listen_cleanup, NULL) ;

    assert_false(worker_init(2));
    assert_false(socket_setup(5001));
    assert_false(socket_poll());

    pthread_cleanup_pop(0);
    return NULL;
}

//accept connection, and disconnect
static void socket_connect(void **state) {

    static pthread_t TestSocket;
    pthread_create(&TestSocket, NULL, test_socket_listen, NULL);

    sleep(1); //give time for socket to be up. just lazy

    CURL *curl = curl_easy_init();
    assert_non_null(curl);
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5001");
    curl_easy_setopt(curl, CURLOPT_SERVER_RESPONSE_TIMEOUT, 0.5L);   //1 sec timeout for connect
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);   //1 sec timeout for connect

    /* https://curl.se/libcurl/c/libcurl-errors.html */
    switch (curl_easy_perform(curl), 1) {
        case CURLE_UNSUPPORTED_PROTOCOL:    // returning just data
        case CURLE_OPERATION_TIMEDOUT:      // return nothing
            break;
        default:
            assert_false(false);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();  // for valgrind

    pthread_cancel(TestSocket);
    pthread_join(TestSocket, NULL);

}

const struct CMUnitTest test_socket[] = {
        cmocka_unit_test(socket_happy_connect),
        cmocka_unit_test(socket_happy_connect2),
        cmocka_unit_test(socket_try_connect),
        cmocka_unit_test(socket_connect),
};
