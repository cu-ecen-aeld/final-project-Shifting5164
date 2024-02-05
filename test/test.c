#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "test_settings.c"
#include "test_logging.c"

int main(void) {
    int iRet = 0;

    iRet += cmocka_run_group_tests(test_settings, NULL, NULL);
    iRet += cmocka_run_group_tests(test_logging, NULL, NULL);

    return iRet;
}
