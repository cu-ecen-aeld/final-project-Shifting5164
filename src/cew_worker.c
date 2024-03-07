#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <ev.h>

#include <cew_settings.h>
#include <cew_worker.h>
#include <cew_logger.h>
#include <cew_client.h>
#include <cew_socket.h>
#include <cew_exit.h>
#include <fcntl.h>


/* Clients queue in thread to serve */
//typedef struct sClientEntry {
//    tsClientStruct *psClient;               // client to serve in the thread
//    STAILQ_ENTRY(sClientEntry) entries;     // Singly linked tail queue
//} tsClientEntry;
//STAILQ_HEAD(ClientQueue, sClientEntry);

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

#define IPC_MAGIC_HEADER ((int16_t)0x936C)
typedef struct sIPCmsg {
    int16_t iHeader;
    int32_t iSize;
    int32_t iFd;
    pid_t Pid;
    uint32_t uiChecksum;
} tsIPCmsg;

typedef struct sWorkerEv {
    ev_io io;
    tsWorkerStruct *psWorker;
} tsWorkerEv;

/* Keep track of everything that happens */
static tsWorkerAdmin gWorkerAdmin = {0};

/* Global ID for the worker process, only available in workers */
static uint32_t guiWorkerID = 0;

//#############################################################################################
//#######       Worker process functions
//#############################################################################################

/* Check the waiting client queue for this thread, get the clients
 * and add them to this thread for serving */
//static int32_t worker_check_waiting(tsWorkerStruct *psArgs) {
//
//    /* Check waiting clients, skip when busy */
//    if (pthread_mutex_trylock(&gWorkerAdmin.Mutex[psArgs->uiId]) == 0) {
//
//        /* Add a new client when something is there to add */
//        tsClientEntry *psClientEntry = STAILQ_FIRST(&psArgs->ClientWaitingQueue);
//        if (psClientEntry) {
//
//            /* Get client, and destroy queue entry */
//            tsClientStruct *psNewClient = psClientEntry->psClient;
//            STAILQ_REMOVE_HEAD(&psArgs->ClientWaitingQueue, entries);
//            free(psClientEntry);
//
//            log_debug("thread %d, Got new client %d from the receive queue.", psArgs->uiId, psNewClient->iId);
//
//            // TMP add to thread queue //todo
//            tsClientEntry *psNewEntry = malloc(sizeof(struct sClientEntry));
//            psNewEntry->psClient = psNewClient;
//            STAILQ_INSERT_TAIL(&psArgs->ClientServingQueue, psNewEntry, entries);
//
//            log_debug("thread %d. Serving new client %d", psArgs->uiId, psNewClient->iId);
//
//            return 0;
//        }
//
//        if (pthread_mutex_unlock(&gWorkerAdmin.Mutex[psArgs->uiId]) != 0) {
//            pthread_exit(NULL);
//        }
//    }
//
//    return -1;
//}

///* Exit the worker process, only by signal handling */
//static void workerp_process_stop(void) {
//    settings_destroy();
//    worker_destroy();
//
//    log_info("Goodbye from worker %d, on pid %d", guiWorkerID, getpid());
//    logger_destroy();
//
//    exit(EXIT_SUCCESS);
//}


//#################################################################
//                               IPC
//#################################################################

/*
    https://en.wikipedia.org/wiki/Adler-32
    where data is the location of the data in physical memory and
    len is the length of the data in bytes
*/
static uint32_t adler32(const uint8_t *data, size_t len) {
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1, b = 0;
    size_t index;

    // Process each byte of the data in order
    for (index = 0; index < len; ++index) {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

/* Checksum calculation over the complete struct sIPCmsg, with
 * uiChecksum init as 0 using adler32
 */
static int32_t set_fd_in_ipc(tsIPCmsg *psIPC, const int32_t *piFd) {
    memset(psIPC, 0, sizeof(struct sIPCmsg));

    psIPC->iHeader = IPC_MAGIC_HEADER;
    psIPC->iSize = sizeof(struct sIPCmsg);
    psIPC->iFd = *piFd;
    psIPC->Pid = getpid();
    psIPC->uiChecksum = adler32((unsigned char *) psIPC, sizeof(struct sIPCmsg));

    return WORKER_EXIT_SUCCESS;
}

static int32_t get_fd_from_ipc(const tsIPCmsg *psIPC, int32_t *piFd, pid_t *Pid) {

    /* Copy because we need to set the checksum to 0 */
    tsIPCmsg sMsg;
    memcpy(&sMsg, psIPC, sizeof(struct sIPCmsg));

    if ((sMsg.iHeader == IPC_MAGIC_HEADER) && (sMsg.iSize == sizeof(struct sIPCmsg))) {

        uint32_t uiOldChecksum = sMsg.uiChecksum;
        sMsg.uiChecksum = 0;
        uint32_t uiNewChecksum = adler32((unsigned char *) &sMsg, sizeof(struct sIPCmsg));

        if (uiOldChecksum == uiNewChecksum) {
            *piFd = psIPC->iFd;
            *Pid = psIPC->Pid;
            log_debug("IPC Received data: %d", psIPC->iFd);
            return WORKER_EXIT_SUCCESS;
        } else {
            log_debug("IPC checksum error");
        }
    } else {
        log_debug("IPC no header or wrong size");
    }

    return WORKER_EXIT_FAILURE;
}

static int32_t read_ipc_from_socket(const tsWorkerStruct *psWorker, int32_t *piFd, pid_t *Pid) {

    tsIPCmsg RecevedIPC = {0};
    int32_t iRead = read(psWorker->iWorkerIPCfd, &RecevedIPC, sizeof(RecevedIPC));

    if (iRead == -1) {
        perror("worker read");
    } else if (iRead >= sizeof(struct sIPCmsg)) {
        if (get_fd_from_ipc(&RecevedIPC, piFd, Pid) == WORKER_EXIT_SUCCESS) {
            return WORKER_EXIT_SUCCESS;
        }
    } else {
        log_debug("worker %d. received something with size %d, but not a ipc msg", psWorker->uiId, iRead);
    }

    return WORKER_EXIT_FAILURE;
}

static int32_t write_ipc_to_socket(const tsWorkerStruct *psWorker, tsIPCmsg *psIPCmsg) {

    if (write(psWorker->iMasterIPCfd, psIPCmsg, sizeof(struct sIPCmsg)) > 0) {
        return WORKER_EXIT_SUCCESS;
    }

    return WORKER_EXIT_FAILURE;
}

//#################################################################
//                     workerp functions
//#################################################################


static void workerp_signal_handler(const int32_t ciSigno) {

    if (ciSigno != SIGTERM) {
        return;
    }

    bTerminateProg = true;
}

/* Setup signals for workers only */
static int32_t workerp_setup_signal(void) {

    /* SIGTERM or SIGTERM terminates the program with cleanup */
    struct sigaction sSigAction;

    sigemptyset(&sSigAction.sa_mask);

    sSigAction.sa_flags = 0;
    sSigAction.sa_handler = workerp_signal_handler;

    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
        return errno;
    }

    return EXIT_SUCCESS;
}


static int32_t workerp_connect_ipc_socket(tsWorkerStruct *psWorker) {

    log_debug("Connecting to IPC socket for worker %d on file %s", psWorker->uiId, psWorker->IPCFile);

    /* Wait for the master to finish setting-up the file socket */
    while (access(psWorker->IPCFile, F_OK) != 0);

    log_debug("worker %d. Master made the socket file available", psWorker->uiId);

    /* Create local socket. */
    /* https://www.man7.org/linux/man-pages/man2/socket.2.html */
    if ((psWorker->iWorkerIPCfd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        perror("Socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_un sName = {0};
    sName.sun_family = AF_UNIX;
    if (sdslen(psWorker->IPCFile) < sizeof(sName.sun_path)) {
        memcpy(sName.sun_path, psWorker->IPCFile, sdslen(psWorker->IPCFile));
    } else {
        perror("memcpy");
        return EXIT_FAILURE;
    }

    /* Connect */
    if ((connect(psWorker->iWorkerIPCfd, (const struct sockaddr *) &sName, sizeof(sName))) == -1) {
        perror("Connect");
        return (EXIT_FAILURE);
    }

    /* non-blocking socket settings */
    if (fcntl(psWorker->iWorkerIPCfd, F_SETFL, O_NONBLOCK) == -1) {
        return errno;
    }

    log_debug("Worker %d, connected with master through fd %d", psWorker->uiId, psWorker->iWorkerIPCfd);

    return EXIT_SUCCESS;
}


static void workerp_ipc_callback(struct ev_loop *loop, ev_io *w_, int revents) {

    tsWorkerEv *psWorkerEv = (tsWorkerEv *) w_;
    tsWorkerStruct *psWorker = psWorkerEv->psWorker;

    int32_t fd;
    pid_t pid;

    read_ipc_from_socket(psWorker, &fd, &pid);

    log_debug("Got callback with fd:%d from pid:%d", fd, pid);

}


/* Serve the clients that are connected to the server.
 * NOTE: this is only called from a fork();
 *
 */
_Noreturn static void workerp_entry(tsWorkerStruct *psWorker) {

    psWorker->iMe = 1;

    /* Destroy the old logger because this is inherited from a different process,
    * re-open the logger to get a working logging thread for this process. */
    logger_destroy();
    tsSSettings sCurrSSettings = settings_get();
    logger_init(sCurrSSettings.pcLogfile, sCurrSSettings.lLogLevel);

    /* Setup new signal handlers for my worker process */
    workerp_setup_signal();

    /* Close everything that we don't need */
    close(psWorker->iMasterIPCAccept);
    psWorker->iMasterIPCAccept = 0;
    close(psWorker->iMasterIPCfd);
    psWorker->iMasterIPCfd = 0;

    psWorker->Pid = getpid();
    guiWorkerID = psWorker->uiId;

    log_debug("Worker %d is running.", psWorker->uiId);

    /* Connect to parent IPC */
    if (workerp_connect_ipc_socket(psWorker) != 0) {
        exit(EXIT_FAILURE);
    }

    struct ev_loop *psLoop;

    psLoop = ev_default_loop(0);
    tsWorkerEv sWorkerEv = {0};
    sWorkerEv.psWorker = psWorker;

    /* Setup the callback for client notification */
    ev_io_init(&sWorkerEv.io, workerp_ipc_callback, psWorker->iWorkerIPCfd, EV_READ);

    ev_io_start(psLoop, &sWorkerEv.io);
    ev_run(psLoop, 0);

}

//
//
//
//    while (1) {
//
//        if (bTerminateProg) {
//            do_exit(0);
//        }
//
//        if (log_debug("I am worker %d", psWorker->uiId) != 0) {
//            exit(EXIT_FAILURE);
//        }
//
//        log_debug("worker %d. Waiting for data.", psWorker->uiId);
//
//        int32_t fd = 0;
//        read_ipc_from_socket(psWorker, &fd);
//        sleep(1);
//    }
//}

//    tsWorkerStruct *psArgs = arg;
//
//    log_info("thread %d. Worker started.", psArgs->uiId);
//
//    //TODO security
//    //chroot
//    // drop privileges ?
//
//    while (1) {
//
//        pthread_testcancel();
//
//        /* Check for waiting clients, and add them to this thread */
//        worker_check_waiting(psArgs);
//
//        /* Do some work commented because cppcheck.*/
//        char testbuff[100] = {0};
//        tsClientEntry *CurrEntry;
//        STAILQ_FOREACH(CurrEntry, &psArgs->ClientServingQueue, entries) {
//            tsClientStruct *psCurrClient = CurrEntry->psClient;
//            snprintf(testbuff, sizeof(testbuff), "thread %u, serving client %i\n", psArgs->uiId, psCurrClient->iId);
//            send(psCurrClient->iSockfd, testbuff, sizeof(testbuff), 0);
//            log_debug("%s", testbuff);
//        }
//
//        sleep(1);
//
//    }
//    pthread_exit(NULL);


//#################################################################
//                     worker functions
//#################################################################


static int32_t worker_create_ipc_socket(tsWorkerStruct *psWorker) {

    log_debug("Creating IPC socket for worker %d, file %s", psWorker->uiId, psWorker->IPCFile);

    /* Create local socket. */
    if ((psWorker->iMasterIPCAccept = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        perror("Socket");
        return EXIT_FAILURE;
    }

    /* Bind socket to socket sName.
     * Make sure the string fits first */
    struct sockaddr_un sName = {0};
    sName.sun_family = AF_UNIX;
    if (sdslen(psWorker->IPCFile) < sizeof(sName.sun_path)) {
        memcpy(sName.sun_path, psWorker->IPCFile, sdslen(psWorker->IPCFile));
    } else {
        perror("memcpy");
        return EXIT_FAILURE;
    }

    /* Make sure the IPC folder exists */
    struct stat st = {0};
    if (stat(WORKER_IPC_FOLDER, &st) == -1) {
        if (mkdir(WORKER_IPC_FOLDER, 0700) == -1) {
            perror("mkdir");
            return EXIT_FAILURE;
        }
    }

    /* In case something was leftover from another startup */
    unlink(psWorker->IPCFile);

    if (bind(psWorker->iMasterIPCAccept, (const struct sockaddr *) &sName, sizeof(sName.sun_path) - 1) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    /* Secure the IPC file */
    if (chmod(psWorker->IPCFile, S_IRUSR | S_IWUSR) == -1) {
        perror("chmod");
        return EXIT_FAILURE;
    }

    if (listen(psWorker->iMasterIPCAccept, 20) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    /* Wait for client worker to accept */
    if ((psWorker->iMasterIPCfd = accept(psWorker->iMasterIPCAccept, NULL, NULL)) == -1) {
        perror("accept");
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

    /* Make IPC package, and send fd to the worker */
    tsIPCmsg sMsg;
    if (set_fd_in_ipc(&sMsg, iSockfd) == WORKER_EXIT_SUCCESS) {
        if (write_ipc_to_socket(psWorker, &sMsg) == WORKER_EXIT_SUCCESS) {
            return WORKER_EXIT_SUCCESS;
        }
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
            if (!psWorker->iMe) {

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

            } else {

                ev_loop_destroy (EV_DEFAULT_UC);        //TODO ??

                log_debug("pid %d, worker: Destroying myself %d from worker pid %d", getpid(), i, psWorker->Pid);
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


_Noreturn void worker_dummy_send(void) {

    while (1) {

        if (bTerminateProg) {
            do_exit(0);
        }

        for (uint32_t i = 0; i < gWorkerAdmin.uiCurrWorkers; i++) {
            if (gWorkerAdmin.psWorker[i]) {
                tsWorkerStruct *psWorker = gWorkerAdmin.psWorker[i];

                if (psWorker->Pid != 0) {

                    tsIPCmsg IPCSend = {0};
                    int32_t fd = 42;
                    set_fd_in_ipc(&IPCSend, &fd);

                    log_debug("Master: Sending data to worker %d. data=%d", i, IPCSend.iFd);

                    int ret = write(psWorker->iMasterIPCfd, &IPCSend, sizeof(IPCSend));
                    if (ret == -1) {
                        perror("write main");
//                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
        sleep(1);
    }
}

#endif