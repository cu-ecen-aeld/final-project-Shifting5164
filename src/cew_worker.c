#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <cew_settings.h>
#include <cew_worker.h>
#include <cew_logger.h>
#include <cew_client.h>


/* Clients queue in thread to serve */
typedef struct sClientEntry {
    tsClientStruct *psClient;               // client to serve in the thread
    STAILQ_ENTRY(sClientEntry) entries;     // Singly linked tail queue
} tsClientEntry;
STAILQ_HEAD(ClientQueue, sClientEntry);

/* Worker struct */
typedef struct sWorkerStruct {
    uint32_t uiId;                           // Worker id
//    int32_t iClientsServing;                // Connected clients
//    pthread_t th;                         // thread
    pid_t Pid;

    /* Clients that are being actively served by the thread
     * Tail append, pop head */
    struct ClientQueue ClientServingQueue;

    /* Clients that are pending to be added to the thread
     * Tail append, pop head */
//    struct ClientQueue ClientWaitingQueue;
} tsWorkerStruct;

/* Worker administration struct */
typedef struct sWorkerAdmin {
    uint32_t uiCurrWorkers;                         // Amount of current workers
    tsWorkerStruct *psWorker[WORKER_MAX_WORKERS];   // Registered workers
//    pthread_mutex_t Mutex[WORKER_MAX_WORKERS];      // Mutex for ClientWaitingQueue communication
} tsWorkerAdmin;

/* Keep track of everything that happens */
static tsWorkerAdmin gWorkerAdmin = {0};

/* Global ID for the workerprocess, only available in workers */
static uint32_t guiWorkerID;

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


/* Exit the worker process */
static void worker_process_stop(void) {
    settings_destroy();
    logger_destroy();
    exit(0);
}

static void worker_signal_handler(const int32_t ciSigno) {

    if (ciSigno != SIGINT && ciSigno != SIGTERM) {
        return;
    }

    log_warning("Worker %d. Got signal: %d", guiWorkerID, ciSigno);

    if (ciSigno == SIGINT) {
        worker_process_stop();
    }
}

/* Setup signals for workers only */
static int32_t worker_setup_signal(void) {

    /* SIGINT or SIGTERM terminates the program with cleanup */
    struct sigaction sSigAction;

    sigemptyset(&sSigAction.sa_mask);

    sSigAction.sa_flags = 0;
    sSigAction.sa_handler = worker_signal_handler;

    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
        return errno;
    }

    return EXIT_SUCCESS;
}

/* Serve the clients that are connected to the server.
 * NOTE: this is only called from a fork();
 *
 */
_Noreturn static void worker_process(uint32_t uiId) {

    /* Destroy the old logger because this is inherited from a different process,
     * re-open the logger to get a working thread for this process. */
    logger_destroy();
    tsSSettings sCurrSSettings = settings_get();
    logger_init(sCurrSSettings.pcLogfile, sCurrSSettings.lLogLevel);

    guiWorkerID = uiId;
    log_debug("Worker %d is running.", uiId);

    /* Setup new signal handlers for my worker process */
    worker_setup_signal();

    while (1) {

        if (log_debug("I am worker %d", uiId) != 0) {
            exit(EXIT_FAILURE);
        }

        sleep(1);
    }


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
}

/* Route an accepted client to a worker that is the least busy. This
 * thread will serve the client with data */
int32_t worker_route_client(tsClientStruct *psClient) {

    static uint32_t NextWorker = -1; //keep track

    if (psClient == NULL) {
        return WORKER_EXIT_FAILURE;
    }

    /* Route the client to a worker (simple round robbin for now)*/
    NextWorker = (NextWorker + 1) % gWorkerAdmin.uiCurrWorkers;
    uint32_t worker = NextWorker;
    worker++; // workaround todo

    log_debug("Adding client %d to worker %d", psClient->iId, NextWorker);

    /* Add client to the queue, free at popping from queue */
    tsClientEntry *psClientEntry = malloc(sizeof(struct sClientEntry));
    memset(psClientEntry, 0, sizeof(struct sClientEntry));
    psClientEntry->psClient = psClient;


    //TODO pid fd pass

//    if (pthread_mutex_lock(&gWorkerAdmin.Mutex[worker]) != 0) {
//        free(psClientEntry);
//        return WORKER_EXIT_FAILURE;
//    }
//
//    STAILQ_INSERT_TAIL(&gWorkerAdmin.psWorker[worker]->ClientWaitingQueue, psClientEntry, entries);
//
//    if (pthread_mutex_unlock(&gWorkerAdmin.Mutex[worker]) != 0) {
//        return WORKER_EXIT_FAILURE;
//    }

    free(psClientEntry);
    return WORKER_EXIT_SUCCESS;
}

/* spinup and configure worker threads
 * NOTE: calloc will break valgrind
 */
int32_t worker_init(const int32_t iWantedWorkers, const char *pcLogfilePath, tLoggerType Loglevel) {

    gWorkerAdmin.uiCurrWorkers = 0;

    for (int32_t i = 0; i < iWantedWorkers; i++) {

        log_debug("Adding worker %d to system.", i);

        /* Allocate mem for worker and thread args */
        if ((gWorkerAdmin.psWorker[i] = malloc(sizeof(struct sWorkerStruct))) == NULL) {
            goto exit_no_worker;
        }
        memset(gWorkerAdmin.psWorker[i], 0, sizeof(struct sWorkerStruct));

        /* Add thread args to pass */
        gWorkerAdmin.psWorker[i]->uiId = i;

        pid_t pid = fork();
        switch (pid) {
            case -1: //error
                log_debug("Error forking for worker %d.", i);
                return WORKER_EXIT_FAILURE;
                break;

            case 0: //child
                /* Goto worker processing */
                worker_process(gWorkerAdmin.psWorker[i]->uiId);

                /* Worker should never exit */
                exit(EXIT_FAILURE);

            default: //parent
                /* Archive pid for monitoring alter */
                gWorkerAdmin.psWorker[i]->Pid = pid;
                log_debug("New worker %d has pid %d.", i, pid);
                break;
        }

//        /* Init mutext */
//        if (pthread_mutex_init(&gWorkerAdmin.Mutex[i], NULL) != 0)
//            goto exit_after_worker;
//
//        /* Init queues */
//        STAILQ_INIT(&gWorkerAdmin.psWorker[i]->ClientWaitingQueue);
//        STAILQ_INIT(&gWorkerAdmin.psWorker[i]->ClientServingQueue);

        /* Spin up worker thread */
//        if (pthread_create(&gWorkerAdmin.psWorker[i]->th, NULL, worker_thread, gWorkerAdmin.psWorker[i]) != 0) {
//            goto exit_after_worker;
//        }

        gWorkerAdmin.uiCurrWorkers++;
        continue;

        /* exit conditions */
        exit_no_worker:
        gWorkerAdmin.psWorker[i] = NULL;

        return WORKER_EXIT_FAILURE;
    }

    return WORKER_EXIT_SUCCESS;
}


//NOTE: calloc will break valgrind
int32_t worker_destroy(void) {

    /* Remove worker entries */
    for (uint32_t i = 0; i < gWorkerAdmin.uiCurrWorkers; i++) {

        if (gWorkerAdmin.psWorker[i]) {

            log_debug("Destroying worker %d.", i);

            /* stop process */
            if (kill(gWorkerAdmin.psWorker[i]->Pid, SIGINT) == 0) {
                log_debug("Successfully killed worker: %d", gWorkerAdmin.psWorker[i]->Pid);
            } else {
                log_debug("Error killing worker: %d", gWorkerAdmin.psWorker[i]->Pid);
            }

            /* Free serving clients queue */
            tsClientEntry *n1, *n2;
            n1 = STAILQ_FIRST(&gWorkerAdmin.psWorker[i]->ClientServingQueue);
            while (n1 != NULL) {
                n2 = STAILQ_NEXT(n1, entries);
                client_destroy(n1->psClient);
                free(n1);   // queue item
                n1 = n2;
            }

            /* free mem */
            free(gWorkerAdmin.psWorker[i]);
            gWorkerAdmin.psWorker[i] = NULL;
        }
    }

    gWorkerAdmin.uiCurrWorkers = 0;

    log_info("Destroyed workers.");

    return WORKER_EXIT_SUCCESS;
}


/* Does nothing yet, only monitoring workers and outputting a log */
_Noreturn void worker_monitor(void) {

    int32_t iMonitoring = gWorkerAdmin.uiCurrWorkers;
    while (1) {
        for (uint32_t i = 0; i < gWorkerAdmin.uiCurrWorkers; i++) {
            int status;
            if (gWorkerAdmin.psWorker[i]->Pid != 0) {
                if (waitpid(gWorkerAdmin.psWorker[i]->Pid, &status, WNOHANG) != 0) {
                    log_error("pid exit :%d", gWorkerAdmin.psWorker[i]->Pid);
                    gWorkerAdmin.psWorker[i]->Pid = 0;
                    iMonitoring--;
                }
            }
        }

        log_debug("Monitoring %d worker processes", iMonitoring);
        sleep(1);
    }
}
