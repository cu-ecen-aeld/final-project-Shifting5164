/* TODO
 * - logger force flush after x time
 * - logger ascii art how it works
 *
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/resource.h>

#include <ev.h>

#include <banned.h>
#include <cew_settings.h>
#include <cew_logger.h>
#include <cew_socket.h>
#include <cew_worker.h>
#include <cew_client.h>
#include <sys/time.h>

/*
https://github.com/cu-ecen-aeld/final-project-Shifting5164
*/

#define SETTINGS_FILE "/work/test/ini/valid_settings.ini"   //todo

#define RET_OK 0 //todo
#define SOCKET_FAIL -2 //todo

bool bTerminateProg = false; /* terminating program gracefully */


/* completing any open connection operations,
 * closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
static void exit_cleanup(void) {
    socket_close();
    worker_destroy();
    logger_destroy();
    settings_destroy();
}

static void do_exit(const int32_t ciExitval) {
    log_info("Goodbye!");
    exit_cleanup();
    exit(ciExitval);
}

/* Signal actions */
void sig_handler(const int32_t ciSigno) {

    if (ciSigno != SIGINT && ciSigno != SIGTERM) {
        return;
    }

    log_warning("Got signal: %d", ciSigno);

    //todo ctlr+c
    if (ciSigno == SIGINT) {
        do_exit(ciSigno);
    }

    bTerminateProg = true;
}

//static void do_thread_exit_with_errno(const int32_t ciLine, const int32_t ciErrno) {
//    log_error("Exit with %d: %s. Line %d.", ciErrno, strerror(ciErrno), ciLine);
//    pthread_exit((void *) ciErrno);
//}

static void do_exit_with_errno(const int32_t ciErrno) {
    log_error("Exit with %d: %s. Line %d.\n", ciErrno, strerror(ciErrno));
    do_exit(ciErrno);
}

/* Description:
 * Setup signals to catch
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
static int32_t setup_signals(void) {

    /* SIGINT or SIGTERM terminates the program with cleanup */
    struct sigaction sSigAction;

    sigemptyset(&sSigAction.sa_mask);

    sSigAction.sa_flags = 0;
    sSigAction.sa_handler = sig_handler;

    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
        return errno;
    }

    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
        return errno;
    }

    return RET_OK;
}

static int32_t daemonize(void) {

    /* Clear file creation mask */
    umask(0);

    /* Get fd limts for later */
    struct rlimit sRlim;
    if (getrlimit(RLIMIT_NOFILE, &sRlim) < 0) {
        log_error("Can't get file limit. Line %d.");
        do_exit(1);
    }

    /* Session leader */
    pid_t pid;
    if ((pid = fork()) < 0) {
        return errno;
    } else if (pid != 0) {
        /* Exit parent */
        exit(EXIT_SUCCESS);
    }
    setsid();

    /* Disallow future opens won't allocate controlling TTY's */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        return errno;
    }

    /* real fork */
    if ((pid = fork()) < 0) {
        return errno;
    } else if (pid != 0) {
        /* Exit parent */
        exit(EXIT_SUCCESS);
    }

    if (chdir("/") < 0) {
        return errno;
    };

    /* Close all fd's */
    if (sRlim.rlim_max == RLIM_INFINITY) {
        sRlim.rlim_max = 1024;
    }

    uint64_t i;
    for (i = 0; i < sRlim.rlim_max; i++) {
        close(i);
    }

    /* Attach fd 0/1/2 to /dev/null */
    int32_t fd0, fd1, fd2;
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        log_error("Error setting up file descriptors.");
        do_exit(1);
    }

    return RET_OK;
}

static void seed_random(void){
    struct timeval t;
    gettimeofday(&t, NULL);
    srand(t.tv_usec * t.tv_sec);
}

int32_t main(int32_t argc, char **argv) {

    bool bDeamonize = false;
    int32_t iRet = 0;

    if ((argc > 1) && strcmp(argv[0], "-d")) {
        bDeamonize = true;
    }

    seed_random();

    if ((iRet = settings_init()) != 0) {
        do_exit_with_errno(iRet);
    }

    if ((iRet = settings_load(SETTINGS_FILE)) != 0) {
        do_exit_with_errno(iRet);
    }

    /* Get a copy of the current settings */
    tsSSettings sCurrSettings = settings_get();

    if ((iRet = logger_init(sCurrSettings.pcLogfile, (tLoggerType) sCurrSettings.lLogLevel)) != 0) {
        do_exit_with_errno(iRet);
    }

    log_error("----- STARTING -------");

    /* Show settings in log */
    settings_to_log();

    /* Going to run as service or not ? */
    if (bDeamonize) {
        printf("Demonizing, listening on port %ld\n", sCurrSettings.lPort);
        if ((iRet = daemonize()) != 0) {
            do_exit_with_errno(iRet);
        }
    }

    if ((iRet = setup_signals()) != RET_OK) {
        do_exit_with_errno(iRet);
    }

    // todo 10 should be settings
    if ((iRet = worker_init(2)) !=
        WORKER_EXIT_SUCCESS) {
        do_exit_with_errno(iRet);
    }

    /* Opens a stream socket, failing and returning -1 if any of the socket connection steps fail. */
    if ((iRet = socket_setup((int16_t) sCurrSettings.lPort)) != SOCK_EXIT_SUCCESS) {
        log_error("Exit with %d: %s. Line %d.\n", iRet, strerror(iRet));
        do_exit(SOCKET_FAIL);
    }

    if (!bDeamonize) {
        printf("Waiting for connections on port %ld...\n", sCurrSettings.lPort);
    }

    worker_monitor();

    /* Accept connecting clients here */
    socket_poll();
    while (1) {
        sleep(100);
    }

    do_exit(EXIT_SUCCESS);
}
