#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <ev.h>

#include <cew_settings.h>
#include <cew_worker.h>
#include <cew_logger.h>
#include <cew_client.h>
#include <cew_socket.h>
#include <cew_exit.h>

/* Worker struct */
typedef struct sWorkerStruct {
    uint32_t uiId;              // Worker id
//    int32_t iClientsServing;  // Connected clients
    pid_t Pid;                  // PID of the worker
    int32_t iMasterIPCAccept;               // master main fd for accepting the client
    int32_t iMasterIPCfd;               // master <> worker IPC
    int32_t iWorkerIPCfd;             // worker <> master IPC
    int32_t iMe;                    // a worker has set the me, so it will only cleanup its zelf, and not others
    sds IPCFile;                // location of IPC unix named socket
} tsWorkerStruct;

/* Worker administration struct */
typedef struct sWorkerAdmin {
    uint32_t uiCurrWorkers;                         // Amount of current workers
    tsWorkerStruct *psWorker[WORKER_MAX_WORKERS];   // Registered workers
} tsWorkerAdmin;

/* ev callback definition */
typedef struct sWorkerEv {
    ev_io io;
    tsWorkerStruct *psWorker;
} tsWorkerEv;

/* Keep track of everything that happens */
static tsWorkerAdmin gWorkerAdmin = {0};

/* Workers default EV loop, used by workers and clients on that worker */
struct ev_loop *psWorkerEVLoop = NULL;

//#################################################################
//                               IPC
//#################################################################
// background info : https://www.sobyte.net/post/2022-01/pass-fd-over-domain-socket/

static int32_t send_fd_to_worker(const tsWorkerStruct *psWorker, const int32_t *piFd) {

    struct sockaddr_un sAd;
    sAd.sun_family = AF_UNIX;

    uint8_t cDst[] = "worker";
    memcpy(sAd.sun_path, cDst, strlen(cDst));

    /* Not sending data */
    struct iovec sData = {0};

    /* Reserve space for one fd */
    uint8_t ucMsg[CMSG_SPACE(sizeof(int32_t))];

    /* Make and fill the msg header */
    struct msghdr sHeader = {(void *) &sAd, sizeof(sAd), &sData, 1, ucMsg, sizeof(ucMsg), 0};
    struct cmsghdr *psHeader = CMSG_FIRSTHDR(&sHeader);
    if (psHeader) {

        psHeader->cmsg_level = SOL_SOCKET;
        psHeader->cmsg_type = SCM_RIGHTS;
        psHeader->cmsg_len = CMSG_LEN(sizeof(int32_t));

        /* set fd */
        *(int32_t *) CMSG_DATA(psHeader) = *piFd;

        if (sendmsg(psWorker->iMasterIPCfd, &sHeader, 0) != -1) {
            return WORKER_EXIT_SUCCESS;
        }
    }

    return WORKER_EXIT_FAILURE;
}

/* On the worker side, read the ipc transmitted from the master, and return the fd and pid */
static int32_t receive_fd_from_worker(const tsWorkerStruct *psWorker, int32_t *piFd) {

    /* Reserve enough space to get a whole package */
    int8_t cBuff[32] = {0};
    struct iovec sData = {cBuff, sizeof(cBuff)};

    /* Space for fd size */
    int8_t cMsg[CMSG_SPACE(sizeof(int32_t))];

    struct msghdr sHeader = {NULL, 0, &sData, 1, cMsg, sizeof(cMsg), 0};

    if (recvmsg(psWorker->iWorkerIPCfd, &sHeader, 0) != -1) {

        /* Distill the fd from the package */
        struct cmsghdr *psHeader = CMSG_FIRSTHDR(&sHeader);

        if (psHeader) {
            *piFd = *(int32_t *) CMSG_DATA(psHeader);
            return WORKER_EXIT_SUCCESS;
        }
    }

    return WORKER_EXIT_FAILURE;
}

//#################################################################
//                     workerp functions
//#################################################################

static int32_t workerp_connect_ipc_socket(tsWorkerStruct *psWorker) {

    log_debug("Connecting to IPC socket for worker %d on file %s", psWorker->uiId, psWorker->IPCFile);

    /* Wait for the master to finish setting-up the file socket */
    while (access(psWorker->IPCFile, F_OK) != 0);

    log_debug("worker %d. Master made the socket file available", psWorker->uiId);

    /* Create local socket. */
    /* https://www.man7.org/linux/man-pages/man2/socket.2.html */
    if ((psWorker->iWorkerIPCfd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        return EXIT_FAILURE;
    }

    struct sockaddr_un sName = {0};
    sName.sun_family = AF_UNIX;
    if (sdslen(psWorker->IPCFile) < sizeof(sName.sun_path)) {
        memcpy(sName.sun_path, psWorker->IPCFile, sdslen(psWorker->IPCFile));
    } else {
        return EXIT_FAILURE;
    }

    /* Connect */
    if ((connect(psWorker->iWorkerIPCfd, (const struct sockaddr *) &sName, sizeof(sName))) == -1) {
        return EXIT_FAILURE;
    }

    /* non-blocking socket settings */
    if (fcntl(psWorker->iWorkerIPCfd, F_SETFL, O_NONBLOCK) == -1) {
        return EXIT_FAILURE;
    }

    log_debug("Worker %d, connected with master through fd %d", psWorker->uiId, psWorker->iWorkerIPCfd);

    return EXIT_SUCCESS;
}

/* Exit signal callback from ev. Kill ev loop */
static void workerp_callback_exitsig(struct ev_loop *loop, ev_signal *w, int revents) {
    ev_break(loop, EVBREAK_ALL);
}

/* receive the IPC from the master, get the fd for the client to serve,
 * and the register a new client.
 */
static void workerp_ipc_callback(struct ev_loop *loop, ev_io *w_, int revents) {
    tsWorkerEv *psWorkerEv = (tsWorkerEv *) w_;
    tsWorkerStruct *psWorker = psWorkerEv->psWorker;

    /* Get the iFd for the new client */
    int32_t iFd;
    if (receive_fd_from_worker(psWorker, &iFd) == WORKER_EXIT_SUCCESS) {
        log_debug("Got callback with iFd:%d", iFd);

        if (client_register_ev(iFd) != EXIT_SUCCESS) {
            log_error("Client registering failed");
        }

    } else {
        log_error("reading from ipc failed !");
    }
}

/* Serve the clients that are connected to the server.
 * NOTE: this is only called from a fork();
 *
 *  https://linux.die.net/man/3/ev
 */
static void workerp_entry(tsWorkerStruct *psWorker) {

    /* Mark psWorker as owned by this worker, needing this for the cleanup later */
    psWorker->iMe = 1;

    /* Destroy the old logger because this is inherited from a different process,
    * re-open the logger to get a working logging thread for this process. */
    logger_destroy();
    tsSSettings sCurrSSettings = settings_get();
    logger_init(sCurrSSettings.pcLogfile, sCurrSSettings.lLogLevel);

    /* Close everything that we don't need */
    close(psWorker->iMasterIPCAccept);
    psWorker->iMasterIPCAccept = 0;
    close(psWorker->iMasterIPCfd);
    psWorker->iMasterIPCfd = 0;

    psWorker->Pid = getpid();

    if (chdir("/") < 0) {
        do_exit(EXIT_FAILURE);
    }

    log_info("Worker %d is running.", psWorker->uiId);

    /* Connect to parent IPC */
    if (workerp_connect_ipc_socket(psWorker) != 0) {
        do_exit(EXIT_FAILURE);
    }

    /* Prepare main ev loop */
    psWorkerEVLoop = ev_default_loop(0);

    /* SIGINT Signal */
    ev_signal exitsig;
    ev_signal_init (&exitsig, workerp_callback_exitsig, SIGINT);
    ev_signal_start(psWorkerEVLoop, &exitsig);

    /* IPC for getting clients */
    tsWorkerEv sWorkerEv = {0};
    sWorkerEv.psWorker = psWorker;
    ev_io_init(&sWorkerEv.io, workerp_ipc_callback, psWorker->iWorkerIPCfd, EV_READ);
    ev_io_start(psWorkerEVLoop, &sWorkerEv.io);

    /* Start serving facilities on this worker */
    ev_run(psWorkerEVLoop, 0);

    log_info("Worker %d done.", psWorker->uiId);

    /* Actually quiting the process and cleanup */
    do_exit(0);
}


//#################################################################
//                     worker functions
//#################################################################


static int32_t worker_create_ipc_socket(tsWorkerStruct *psWorker) {

    log_debug("Creating IPC socket for worker %d, file %s", psWorker->uiId, psWorker->IPCFile);

    /* Create local socket. */
    if ((psWorker->iMasterIPCAccept = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        return EXIT_FAILURE;
    }

    /* Bind socket to socket sName.
     * Make sure the string fits first */
    struct sockaddr_un sName = {0};
    sName.sun_family = AF_UNIX;
    if (sdslen(psWorker->IPCFile) < sizeof(sName.sun_path)) {
        memcpy(sName.sun_path, psWorker->IPCFile, sdslen(psWorker->IPCFile));
    } else {
        return EXIT_FAILURE;
    }

    /* Make sure the IPC folder exists */
    struct stat st = {0};
    if (stat(WORKER_IPC_FOLDER, &st) == -1) {
        if (mkdir(WORKER_IPC_FOLDER, 0700) == -1) {
            return EXIT_FAILURE;
        }
    }

    /* In case something was leftover from another startup */
    unlink(psWorker->IPCFile);

    if (bind(psWorker->iMasterIPCAccept, (const struct sockaddr *) &sName, sizeof(sName.sun_path) - 1) == -1) {
        return EXIT_FAILURE;
    }

    /* Secure the IPC file */
    if (chmod(psWorker->IPCFile, S_IRUSR | S_IWUSR) == -1) {
        return EXIT_FAILURE;
    }

    if (listen(psWorker->iMasterIPCAccept, 20) == -1) {
        return EXIT_FAILURE;
    }

    /* Wait for client worker to accept */
    if ((psWorker->iMasterIPCfd = accept(psWorker->iMasterIPCAccept, NULL, NULL)) == -1) {
        exit(EXIT_FAILURE);
    }

    log_info("Created IPC socket for worker %d, with fd %d", psWorker->uiId, psWorker->iMasterIPCfd);

    return EXIT_SUCCESS;
}

/* Route an accepted client to a worker that is the least busy. This
 * thread will serve the client with data */
int32_t worker_route_client(int32_t *iSockfd) {

    static uint32_t NextWorker = -1; //keep track

    /* Route the client to a worker (simple round robbin for now)*/
    NextWorker = (NextWorker + 1) % gWorkerAdmin.uiCurrWorkers;
    tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[NextWorker];

    log_debug("Adding new client to worker %d", NextWorker);

    /* Send to worker */
    if (send_fd_to_worker(psWorker, iSockfd) == WORKER_EXIT_SUCCESS) {
        return WORKER_EXIT_SUCCESS;
    }

    return WORKER_EXIT_FAILURE;
}

/* spinup and configure worker threads
 * NOTE: calloc will break valgrind
 */
int32_t worker_init(const int32_t iWantedWorkers) {

    /* safety clean */
    memset(&gWorkerAdmin, 0, sizeof(struct sWorkerAdmin));

    gWorkerAdmin.uiCurrWorkers = 0;

    for (int32_t i = 0; i < iWantedWorkers; i++) {

        log_debug("Adding worker %d to system.", i);

        /* Allocate mem for worker and thread args */
        if ((gWorkerAdmin.psWorker[i] = malloc(sizeof(struct sWorkerStruct))) == NULL) {
            goto exit_no_worker;
        }
        tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[i];
        memset(psWorker, 0, sizeof(struct sWorkerStruct));

        log_debug("psWorker %d @ 0x%X", i, psWorker);

        /* Add thread args to pass */
        psWorker->uiId = i;

        /* Define worker IPC file */
        psWorker->IPCFile = sdscatprintf(sdsempty(), "%s/%s_%ld", WORKER_IPC_FOLDER, WORKER_IPC_FILE, random());

        /* flush logger, because after the child forks we lose the pthread */
        logger_flush();

        /* Spin-up a worker child process */
        pid_t pid = fork();
        switch (pid) {
            case -1: //error
                log_error("Error forking for worker %d.", i);
                goto exit_free_worker;

            case 0: //child
                /* Goto worker processing */
                log_debug("fork: I am PID %d", getpid());
                workerp_entry(psWorker);

                /* Worker should never exit */
                exit(EXIT_FAILURE);

            default: //parent
                /* Archive pid for monitoring alter */
                psWorker->Pid = pid;
                log_debug("New worker %d has pid %d.", i, pid);
                break;
        }

        /* Create IPC for child worker */
        if (worker_create_ipc_socket(psWorker) != 0) {
            goto exit_free_worker;
        }

        gWorkerAdmin.uiCurrWorkers++;
        continue;

        /* Exit conditions */
        exit_free_worker:
        free(gWorkerAdmin.psWorker[i]);
        gWorkerAdmin.psWorker[i] = 0;

        exit_no_worker:
        psWorker = NULL;

        return WORKER_EXIT_FAILURE;
    }

    log_info("Finished with spinning-up %d workers!", gWorkerAdmin.uiCurrWorkers);

    return WORKER_EXIT_SUCCESS;
}

//NOTE: calloc will break valgrind
// will be called from master and worker
// reentrent function
int32_t worker_destroy(void) {

    /* Remove worker entries.
     * NOTE: taking WORKER_MAX_WORKERS here to make sure every possible worker is cleaned, even
     * if something remains after a bad init, and a good destroy from the worker itself. */
    for (uint32_t i = 0; i < WORKER_MAX_WORKERS; i++) {
        tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[i];

        if (psWorker) {

            /* Only the main process may kill workers, not the workers themselfs. But a
             * worker should cleanup all the other mess it inherited from the fork(). */
            if (psWorker->iMe) {

                /* Probably already did this, just to make sure */
                ev_suspend(psWorkerEVLoop);
                ev_loop_destroy(psWorkerEVLoop);

                if (psWorkerEVLoop != NULL) {
                    free(psWorkerEVLoop);
                    psWorkerEVLoop = NULL;
                }

                log_debug("pid %d, worker: Destroying myself %d from worker pid %d", getpid(), i, psWorker->Pid);

            } else {

                log_info("pid %d, master: Destroying worker %d from master.", getpid(), i);
                /* Stop worker process, let it do its own cleanup */
                if (kill(psWorker->Pid, SIGTERM) == 0) {
                    log_debug("pid %d, master: Successfully stopped worker: %d", getpid(), psWorker->Pid);
                } else {
                    log_error("pid %d, master: Error stopping worker: %d. killing it forcefully now!", getpid(),
                              psWorker->Pid);
                    kill(psWorker->Pid, SIGKILL);
                }

                /* Get latest exit info, avoiding zombie, and for information */
                int status;
                waitpid(psWorker->Pid, &status, WUNTRACED | WCONTINUED);
                log_debug("pid %d, master: Exit info from pid %d:%d", getpid(), psWorker->Pid, status);

                /* Cleanup IPC */
                if (psWorker->iMasterIPCfd != 0) {
                    close(psWorker->iMasterIPCfd);
                    psWorker->iMasterIPCfd = 0;
                }

                if (psWorker->iMasterIPCAccept != 0) {
                    close(psWorker->iMasterIPCAccept);
                    psWorker->iMasterIPCAccept = 0;
                }

                /* remove IPC file */
                unlink(psWorker->IPCFile);

            }

            if (psWorker->iWorkerIPCfd != 0) {
                close(psWorker->iWorkerIPCfd);
                psWorker->iWorkerIPCfd = 0;
            }

            if (psWorker->IPCFile != NULL) {
                sdsfree(psWorker->IPCFile);
                psWorker->IPCFile = NULL;
            }

            log_info("pid %d, Destroyed worker %d.", getpid(), psWorker->uiId);

            /* free mem */
            free(gWorkerAdmin.psWorker[i]);
            gWorkerAdmin.psWorker[i] = NULL;
        }
    }

    gWorkerAdmin.uiCurrWorkers = 0;

    log_info("pid %d, Destroyed all workers. All done!", getpid());

    return WORKER_EXIT_SUCCESS;
}

//#################################################################
//                     worker debug functions
//#################################################################

#ifdef INCLUDE_WORKER_DEBUG_FUNCTIONS

/* Does nothing yet, only monitoring workers and outputting a log */
_Noreturn void worker_monitor(void) {

    int32_t iMonitoring = gWorkerAdmin.uiCurrWorkers;
    log_debug("Trying to monitor %d workers.", iMonitoring);
    while (1) {

        if (bTerminateProg) {
            do_exit(0);
        }

        for (uint32_t i = 0; i < gWorkerAdmin.uiCurrWorkers; i++) {
            if (gWorkerAdmin.psWorker[i]) {
                tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[i];
                if (psWorker->Pid != 0) {
                    int status;
                    if (waitpid(psWorker->Pid, &status, WNOHANG) != 0) {
                        log_error("pid exit :%d", psWorker->Pid);
                        psWorker->Pid = 0;
                        iMonitoring--;
                    }
                }
            }
        }

        log_debug("Monitoring %d worker processes", iMonitoring);

        sleep(1);
    }
}

//
//_Noreturn void worker_dummy_send(void) {
//
//    while (1) {
//
//        if (bTerminateProg) {
//            do_exit(0);
//        }
//
//        for (uint32_t i = 0; i < gWorkerAdmin.uiCurrWorkers; i++) {
//            if (gWorkerAdmin.psWorker[i]) {
//                tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[i];
//
//                if (psWorker->Pid != 0) {
//
//                    tsIPCmsg IPCSend = {0};
//                    int32_t fd = 42;
//                    set_fd_in_ipc(&IPCSend, &fd);
//
//                    log_debug("Master: Sending data to worker %d. data=%d", i, IPCSend.iFd);
//
//                    int ret = write(psWorker->iMasterIPCfd, &IPCSend, sizeof(IPCSend));
//                    if (ret == -1) {
//                        perror("write main");
////                        exit(EXIT_FAILURE);
//                    }
//                }
//            }
//        }
//        sleep(1);
//    }
//}

#endif

