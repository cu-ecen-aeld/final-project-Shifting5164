#include <stdlib.h>
#include <string.h>

#include <banned.h>
#include <cew_exit.h>
#include <cew_settings.h>
#include <cew_logger.h>
#include <cew_socket.h>
#include <cew_worker.h>
#include <cew_client.h>

bool bTerminateProg = false;

static void exit_cleanup(void) {
    socket_close();
    worker_destroy();
    settings_destroy();
    logger_destroy();
}

void do_exit(const int32_t ciExitval) {
    log_info("Goodbye pid %d!", getpid());
    exit_cleanup();
    exit(ciExitval);
}

//static void do_thread_exit_with_errno(const int32_t ciLine, const int32_t ciErrno) {
//    log_error("Exit with %d: %s. Line %d.", ciErrno, strerror(ciErrno), ciLine);
//    pthread_exit((void *) ciErrno);
//}

void do_exit_with_errno(const int32_t ciErrno) {
    log_error("Exit pid %d with %d: %s. Line %d.\n",getpid(), ciErrno, strerror(ciErrno));
    do_exit(ciErrno);
}

