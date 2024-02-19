#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>

#include <cew_worker.h>
#include <cew_logger.h>
#include <cew_client.h>

typedef struct sThreadArgs {
    uint32_t uiId;        // Worker id
} tsThreadArgs;

typedef struct sWorkerStruct {
    int32_t iClientsServing;
    pthread_t th;
    tsClientStruct *psClient[WORKER_MAX_CLIENTS];
    tsThreadArgs *psArgs;
} tsWorkerStruct;

typedef struct sWorkerAdmin {
    uint32_t uiCurrWorkers;
    tsWorkerStruct *psWorker[WORKER_MAX_WORKERS];
} tsWorkerAdmin;

static tsWorkerAdmin WorkerAdmin = {0};


static void *worker_thread(void *arg) {

    tsThreadArgs *psArgs = arg;

    printf("Worker thread %ud started.\n", psArgs->uiId);
    pthread_exit(0);

    //TODO security
    //chroot
    // drop privileges ?

    while (1) {
        sleep(1);

        //accept clients

        // ditch clients

        // loop clients

    }

    exit(0);
}

//
//static worker_add_client(void) {
//
//}
//
//static worker_remove_client(void) {
//    // callback from timeout
//
//    // callback from closed connection
//
//}
//
//
//// call from libev, got new client to serve
//// route client to worker that is the least busy
//int32_t worker_route_client(const tsClientStruct *pcClient) {
//
//    // get client
//
////    figure out the destination thread
//
//    // route it
//
//    return WORKER_EXIT_SUCCESS;
//}

int32_t worker_test(const int32_t iWantedWorkers) {

    for (int32_t i = 0; i < iWantedWorkers; i++) {

        WorkerAdmin.psWorker[i] = NULL;

        printf("&WorkerAdmin.psWorker[%d]:%p\n", i, (void *) &WorkerAdmin.psWorker[i]);
        printf("WorkerAdmin.psWorker[%d]:%p\n", i, (void *) WorkerAdmin.psWorker[i]);

        WorkerAdmin.psWorker[i] = (tsWorkerStruct *) malloc(sizeof(struct sWorkerStruct));
        memset(WorkerAdmin.psWorker[i], 0, sizeof(struct sWorkerStruct));

        printf("sizeof(struct sWorkerStruct):0x%x\n", (int) sizeof(struct sWorkerStruct));
        printf("WorkerAdmin.psWorker[%d]:%p\n", i, (void *) WorkerAdmin.psWorker[i]);
        printf("WorkerAdmin.psWorker[%d]->psArgs:%p\n", i, (void *) WorkerAdmin.psWorker[i]->psArgs);
        printf("&WorkerAdmin.psWorker[%d]->psArgs:%p\n", i, (void *) &WorkerAdmin.psWorker[i]->psArgs);

        WorkerAdmin.psWorker[i]->psArgs = malloc(sizeof(struct sThreadArgs));
        memset(WorkerAdmin.psWorker[i]->psArgs, 0, sizeof(struct sThreadArgs));
        printf("WorkerAdmin.psWorker[%d]:%p\n", i, (void *) WorkerAdmin.psWorker[i]);
        printf("WorkerAdmin.psWorker[%d]->psArgs:%p\n", i, (void *) WorkerAdmin.psWorker[i]->psArgs);

        WorkerAdmin.psWorker[i]->psArgs->uiId = 42 + i;
        printf("WorkerAdmin.psWorker[%d]->psArgs->uiId:%p\n", i, (void *) &WorkerAdmin.psWorker[i]->psArgs->uiId);
        printf("WorkerAdmin.psWorker[%d]->psArgs->uiId:%d\n", i, WorkerAdmin.psWorker[i]->psArgs->uiId);

        free(WorkerAdmin.psWorker[i]->psArgs);
        free(WorkerAdmin.psWorker[i]);

        printf("----\n");

    }

    return 0;
}

// spinup and configure worker threads
//NOTE: calloc will break valgrind
int32_t worker_init(const int32_t iWantedWorkers) {

    for (int32_t i = 0; i < iWantedWorkers; i++) {

        /* Allocate mem for worker and thread args */
        if ((WorkerAdmin.psWorker[i] = malloc(sizeof(struct sWorkerStruct))) == NULL)
            goto exit_no_worker;
        memset(WorkerAdmin.psWorker[i], 0, sizeof(struct sWorkerStruct));


        if ((WorkerAdmin.psWorker[i]->psArgs = malloc(sizeof(struct sThreadArgs))) == NULL)
            goto exit_no_args;
        memset(WorkerAdmin.psWorker[i]->psArgs, 0, sizeof(struct sThreadArgs));

        WorkerAdmin.psWorker[i]->psArgs->uiId = i;

        /* Spin up worker thread */
        if (pthread_create(&WorkerAdmin.psWorker[i]->th, NULL, worker_thread, WorkerAdmin.psWorker[i]->psArgs) != 0) {
            goto exit_no_th_spinup;
        }

        WorkerAdmin.uiCurrWorkers++;
        continue;

        /* exit conditions */

        exit_no_th_spinup:
        free(WorkerAdmin.psWorker[i]->psArgs);

        exit_no_args:
        free(WorkerAdmin.psWorker[i]);

        exit_no_worker:
        WorkerAdmin.psWorker[i] = NULL;

        printf("fail\n");
        return WORKER_EXIT_FAILURE;

    }

    return WORKER_EXIT_SUCCESS;

}

//NOTE: calloc will break valgrind
int32_t worker_destroy(void) {

    /* Stop workers */
    //cancel
    //join

    /* Remove worker entries */
    for (uint32_t i = 0; i < WorkerAdmin.uiCurrWorkers; i++) {

        if (WorkerAdmin.psWorker[i]) {
            /* stop threads */
            pthread_cancel(WorkerAdmin.psWorker[i]->th);
            pthread_join(WorkerAdmin.psWorker[i]->th, NULL);

            /* free mem */
            free(WorkerAdmin.psWorker[i]->psArgs);
            free(WorkerAdmin.psWorker[i]);
            WorkerAdmin.psWorker[i] = NULL;
        }
    }

    WorkerAdmin.uiCurrWorkers = 0;

    return WORKER_EXIT_SUCCESS;
}
