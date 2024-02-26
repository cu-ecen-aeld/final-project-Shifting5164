/* Only worker process code */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <cew_logger.h>
#include <cew_settings.h>
#include <cew_socket.h>

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


/* Exit the worker process, only by signal handling */
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

static void worker_setup_ipc(void){

}

/* Serve the clients that are connected to the server.
 * NOTE: this is only called from a fork();
 *
 */
_Noreturn static void worker_process_entry(uint32_t uiId) {

    /* Destroy the old logger because this is inherited from a different process,
     * re-open the logger to get a working thread for this process. */
    logger_destroy();
    socket_close();

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