#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
//#include "../src/cew_exit.c"


static void misc_exit_cleanup(void **state) {
//    exit_cleanup();
}


const struct CMUnitTest test_misc[] = {
        cmocka_unit_test(misc_exit_cleanup),
};