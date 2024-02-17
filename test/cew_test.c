#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "cew_test_settings.c"
#include "cew_test_logging.c"
#include "cew_test_socket.c"

int main(void) {
    int iRet = 0;

    iRet += cmocka_run_group_tests(test_settings, NULL, NULL);
    iRet += cmocka_run_group_tests(test_logging, NULL, NULL);
    iRet += cmocka_run_group_tests(test_socket, NULL, NULL);

    return iRet;
}
