#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>

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
    int32_t iClientsServing;                // Connected clients
    pthread_t th;                           // thread

    /* Clients that are being actively served by the thread
     * Tail append, pop head */
    struct ClientQueue ClientServingQueue;

    /* Clients that are pending to be added to the thread
     * Tail append, pop head */
    struct ClientQueue ClientWaitingQueue;
} tsWorkerStruct;

/* Worker administration struct */
typedef struct sWorkerAdmin {
    uint32_t uiCurrWorkers;                         // Amount of current workers
    tsWorkerStruct *psWorker[WORKER_MAX_WORKERS];   // Registered workers
    pthread_mutex_t Mutex[WORKER_MAX_WORKERS];      // Mutex for ClientWaitingQueue communication
} tsWorkerAdmin;

/* Keep track of everything that happens */
static tsWorkerAdmin WorkerAdmin = {0};

/* Serve the clients that are connected to the server
 *
 */
static void *worker_thread(void *arg) {

    tsWorkerStruct *psArgs = arg;
    tsClientEntry *CurrClient;

    log_info("thread %d. Worker started.", psArgs->uiId);

    //TODO security
    //chroot
    // drop privileges ?

    while (1) {

        pthread_testcancel();

        tsClientStruct *psNewClient = NULL;

        /* Check waiting clients, skip when busy */
        if (pthread_mutex_trylock(&WorkerAdmin.Mutex[psArgs->uiId]) == 0) {

            /* Add a new client when something is there to add */
            tsClientEntry *Entry = STAILQ_FIRST(&psArgs->ClientWaitingQueue);
            if(Entry) {
                psNewClient = Entry->psClient;
                STAILQ_REMOVE_HEAD(&psArgs->ClientWaitingQueue, entries);
                free(Entry);

                log_debug("thread %d, Got new client %d from the receive queue.", psArgs->uiId, psNewClient->iId);
            }

            if (pthread_mutex_unlock(&WorkerAdmin.Mutex[psArgs->uiId]) != 0) {
                pthread_exit(NULL);
            }
        } else {
            pthread_exit(NULL);
        }

        /* If new client, add it to the serving list */
        if (psNewClient != NULL) {
            tsClientEntry *psNewEntry = malloc(sizeof(struct sClientEntry));
            psNewEntry->psClient = psNewClient;
            STAILQ_INSERT_TAIL(&psArgs->ClientServingQueue, psNewEntry, entries);
            log_debug("thread %d. Serving new client %d", psArgs->uiId, psNewClient->iId);
        }

        /* Do some work */
        STAILQ_FOREACH(CurrClient, &psArgs->ClientServingQueue, entries)
            printf("thread %d, serving client %i\n",psArgs->uiId, CurrClient->psClient->iId);

        sleep(1);

    }

    pthread_exit(NULL);
}

// call from libev, got new client to serve
// route client to worker that is the least busy
int32_t worker_route_client(tsClientStruct *psClient) {

    static uint32_t NextWorker = -1; //keep track

    /* Route the client (simple round robbin for now)*/
    NextWorker = (NextWorker + 1) % WorkerAdmin.uiCurrWorkers;
    uint32_t worker = NextWorker;

    log_debug("Adding client %d to worker %d", psClient->iId, NextWorker);

    /* Add client to the queue, free at popping from queue */
    tsClientEntry *psClientEntry = malloc(sizeof(struct sClientEntry));
    memset(psClientEntry, 0, sizeof(struct sClientEntry));
    psClientEntry->psClient = psClient;

    if (pthread_mutex_lock(&WorkerAdmin.Mutex[worker]) != 0) {
        free(psClientEntry);
        return WORKER_EXIT_FAILURE;
    }

    STAILQ_INSERT_TAIL(&WorkerAdmin.psWorker[worker]->ClientWaitingQueue, psClientEntry, entries);

    if (pthread_mutex_unlock(&WorkerAdmin.Mutex[worker]) != 0) {
        return WORKER_EXIT_FAILURE;
    }

    return WORKER_EXIT_SUCCESS;

}

// spinup and configure worker threads
//NOTE: calloc will break valgrind
int32_t worker_init(const int32_t iWantedWorkers) {

    for (int32_t i = 0; i < iWantedWorkers; i++) {

        log_debug("Adding worker %d to system.", i);

        /* Allocate mem for worker and thread args */
        if ((WorkerAdmin.psWorker[i] = malloc(sizeof(struct sWorkerStruct))) == NULL)
            goto exit_no_worker;
        memset(WorkerAdmin.psWorker[i], 0, sizeof(struct sWorkerStruct));

        /* Add thread args to pass */
        WorkerAdmin.psWorker[i]->uiId = i;

        /* Init mutext */
        if (pthread_mutex_init(&WorkerAdmin.Mutex[i], NULL) != 0)
            goto exit_after_worker;

        /* Init queues */
        STAILQ_INIT(&WorkerAdmin.psWorker[i]->ClientWaitingQueue);
        STAILQ_INIT(&WorkerAdmin.psWorker[i]->ClientServingQueue);

        /* Spin up worker thread */
        if (pthread_create(&WorkerAdmin.psWorker[i]->th, NULL, worker_thread, WorkerAdmin.psWorker[i]) != 0) {
            goto exit_after_worker;
        }

        WorkerAdmin.uiCurrWorkers++;
        continue;

        /* exit conditions */
        exit_after_worker:
        free(WorkerAdmin.psWorker[i]);

        exit_no_worker:
        WorkerAdmin.psWorker[i] = NULL;

        return WORKER_EXIT_FAILURE;
    }

    return WORKER_EXIT_SUCCESS;
}

//NOTE: calloc will break valgrind
int32_t worker_destroy(void) {

    /* Remove worker entries */
    for (uint32_t i = 0; i < WorkerAdmin.uiCurrWorkers; i++) {

        if (WorkerAdmin.psWorker[i]) {

            log_debug("Destroying worker %d.", i);

            /* stop threads */
            pthread_cancel(WorkerAdmin.psWorker[i]->th);
            pthread_join(WorkerAdmin.psWorker[i]->th, NULL);

            pthread_mutex_destroy(&WorkerAdmin.Mutex[i]);

            /* Free serving clients queue */
            tsClientEntry *n1, *n2;
            n1 = STAILQ_FIRST(&WorkerAdmin.psWorker[i]->ClientServingQueue);
            while (n1 != NULL) {
                n2 = STAILQ_NEXT(n1, entries);
                free(n1->psClient);
                free(n1);   // queue item
                n1 = n2;
            }

            /* Free waiting clients and queue */
            n1 = STAILQ_FIRST(&WorkerAdmin.psWorker[i]->ClientWaitingQueue);
            while (n1 != NULL) {
                n2 = STAILQ_NEXT(n1, entries);
                free(n1->psClient);
                free(n1);   // queue item
                n1 = n2;
            }

            /* free mem */
            free(WorkerAdmin.psWorker[i]);
            WorkerAdmin.psWorker[i] = NULL;
        }
    }

    WorkerAdmin.uiCurrWorkers = 0;

    log_info("Destroyed workers.");

    return WORKER_EXIT_SUCCESS;
}
