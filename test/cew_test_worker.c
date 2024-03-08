#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/cew_worker.c"
#include "../src/cew_client.c"
#include "../include/cew_logger.h"
#include "../include/cew_client.h"

static char worker_testfile[] = "/var/tmp/worker_testlog";

//happy flow, memcheck
static void worker_happy_init(void **state) {

    unlink(worker_testfile);

    assert_false(logger_init(worker_testfile, eDEBUG));
    assert_false(worker_init(5));

    assert_false(worker_destroy());
    assert_false(logger_destroy());

}
//
//static void worker_ipc_good(void **state) {
//
//    uint8_t socket[] = "/run/testsock";
//    tsWorkerStruct psWorkerSend = {0};
//    psWorkerSend.IPCFile = sdscatprintf(sdsempty(), "%s", socket);
//
//    unlink(socket);
//    assert_false(worker_create_ipc_socket(&psWorkerSend));
//
//    int32_t fd_send = 42;
//    assert_false(send_fd_to_worker(&psWorkerSend, &fd_send));
//
//    //---------
//
//    int32_t fd_receive;
//    tsWorkerStruct psWorkerReceive = {0};
//    psWorkerReceive.IPCFile = sdscatprintf(sdsempty(), "%s", socket);
//    assert_false(receive_fd_from_worker(&psWorkerReceive, &fd_receive));
//
//    assert_int_not_equal(fd_receive, 42);
//
//    sdsfree(psWorkerSend.IPCFile);
//    sdsfree(psWorkerReceive.IPCFile);
//}

//
//static void worker_ipc_bad(void **state) {
//    tsIPCmsg sIPCGood, sIPCBad;
//    int32_t iFD = 42;
//    pid_t pid;
//
//    assert_false(set_fd_in_ipc(&sIPCGood, &iFD));
//
//    // bad header
//    memcpy(&sIPCBad, &sIPCGood, sizeof(sIPCBad));
//    sIPCBad.iHeader = 1;
//    assert_true(get_fd_from_ipc(&sIPCBad, &iFD, &pid));
//
//    // bad size
//    memcpy(&sIPCBad, &sIPCGood, sizeof(sIPCBad));
//    sIPCBad.iSize = 1;
//    assert_true(get_fd_from_ipc(&sIPCBad, &iFD, &pid));
//
//    // bad payload
//    memcpy(&sIPCBad, &sIPCGood, sizeof(sIPCBad));
//    sIPCBad.iFd = 1;
//    assert_true(get_fd_from_ipc(&sIPCBad, &iFD, &pid));
//
//    // bad pid
//    memcpy(&sIPCBad, &sIPCGood, sizeof(sIPCBad));
//    sIPCBad.Pid = 1;
//    assert_true(get_fd_from_ipc(&sIPCBad, &iFD, &pid));
//
//    // bad checksum
//    memcpy(&sIPCBad, &sIPCGood, sizeof(sIPCBad));
//    sIPCBad.uiChecksum = 1;
//    assert_true(get_fd_from_ipc(&sIPCBad, &iFD, &pid));
//}

const struct CMUnitTest test_worker[] = {
        cmocka_unit_test(worker_happy_init),
//        cmocka_unit_test(worker_ipc_good),
//        cmocka_unit_test(worker_ipc_bad),
};
