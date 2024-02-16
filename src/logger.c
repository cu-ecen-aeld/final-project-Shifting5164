#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <sds.h>

#include "logger.h"

static sds gpcLogfile = NULL;
static pthread_t LoggerThread;
static pthread_mutex_t pLogMutex = PTHREAD_MUTEX_INITIALIZER;

/* Mapping from logging level enum to text */
static const char *pcLogTypeMsg[] = {
        "Error",
        "Warning",
        "Info",
        "Debug",
};

/* Log message queue */
typedef struct sQEntry {
    sds msg;   /* A sds pointer to log message */
    STAILQ_ENTRY(sQEntry) entries;        /* Singly linked tail queue */
} tsEntry;
STAILQ_HEAD(stailhead, sQEntry);
static struct stailhead Head;


// Amount of messages in the queue
static int32_t iDataInQueueCount = 0;

// Set when logging system has been setup
static int32_t iIsInitDone = 0;

// Force a flush of the message queue to the datafile
static int32_t iDoForceFlush = 0;

// Dynamic logging level
static int32_t iCurrLogLevel = eWARNING;

/*  Log writing thread.
 *
 * Will try to write the queued logging messages in bulk defined by BULK_WRITE.
 *
 * Args: None
 * Return: errno
 */
static void *logger_thread(void *arg) {

    static FILE *fd = NULL;

    while (1) {

        /* Thread may canceled before and after the sleep, no mallocs or open fd
         * thus no cleanup needed. Can take some extra time tho.*/
        pthread_testcancel();
        usleep(POLLING_INTERVAL);
        pthread_testcancel();

        if ((iDataInQueueCount > BULK_WRITE) || (iDoForceFlush && iDataInQueueCount)) {

            /* Open file for writing */
            if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
                exit(errno);
            }

            /* Bulk write */
            do {

                /* Get data from head, save the log message pointer for later use */
                sds LogMessage;
                tsEntry *Entry = STAILQ_FIRST(&Head);
                LogMessage = Entry->msg;

                // mutex block, only for removing the entry
                {
                    if (pthread_mutex_lock(&pLogMutex) != 0) {
                        exit(errno);
                    }

                    /* Delete entry from the head */
                    STAILQ_REMOVE_HEAD(&Head, entries);
                    free(Entry);
                    iDataInQueueCount--;

                    if (pthread_mutex_unlock(&pLogMutex) != 0) {
                        exit(errno);
                    }
                }

                /* Write the log entry */
                fwrite(LogMessage, sdslen(LogMessage), 1, fd);
                if (ferror(fd) != 0) {
                    exit(errno);
                }

                /* Free the allocated message */
                sdsfree(LogMessage);

            } while (iDataInQueueCount);

            /* Make sure we only flush once when set */
            iDoForceFlush = 0;

            /* Flush and close file after bulk write */
            if (fflush(fd) != 0) {
                exit(errno);
            }

            if (fclose(fd) != 0) {
                exit(errno);
            }
        }
    }
}

/* Add a log message to the writing queue. Writing will be outsourced to a writing thread.
 *
 * Return: errno on error, else 0
 */
static int32_t log_add_to_queue(const tLoggerType eType, const char *pcMsg, va_list args) {

    /* Check max allowed messages in the queue against runaway */
    if (iDataInQueueCount > LOGGER_QUEUE_SIZE) {
        return ENOBUFS;
    }

    /* Add timestamp, IEC compatible */
    char acTime[64];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tmp);

    /* Parse the users message */
    char cUserMsg[LOGGER_MAX_USER_MSG_LEN] = {0};
    vsnprintf(cUserMsg, sizeof(cUserMsg), pcMsg, args);

    /* Make the log entry, dynamic memory for passing into the queue.
     * free @ logger_thread */
    sds LogMsg = sdscatprintf(sdsempty(), "%s : %s : %s\n", acTime, pcLogTypeMsg[eType], cUserMsg);

    /* Pack message in a queue entry
     * free @ logger_thread
     * */
    tsEntry *NewEntry = malloc(sizeof(tsEntry));
    NewEntry->msg = LogMsg;

    /* mutex safe block */
    {
        if (pthread_mutex_lock(&pLogMutex) != 0) {
            sdsfree(LogMsg);
            free(NewEntry);
            return errno;
        }

        /* Push at the tail, using head for popping */
        STAILQ_INSERT_TAIL(&Head, NewEntry, entries);
        iDataInQueueCount++;

        if (pthread_mutex_unlock(&pLogMutex) != 0) {
            return errno;
        }
    }

    return 0;
}

/* Write message on this level
 *
 * Return
 * - -1 when logging system is no init yet
 * - -2 when current level is not sufficient to queue the message
 * - 0 when message is queued sucesfully
 * - errno on error
 */
int32_t log_debug(const char *message, ...) {

    if (!iIsInitDone) {
        return -1;
    }

    if (iCurrLogLevel < eDEBUG) {
        return -2;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eDEBUG, message, args);
    va_end(args);

    return ret;
}

/* Write message on this level
 *
 * Return
 * - -1 when logging system is no init yet
 * - -2 when current level is not sufficient to queue the message
 * - 0 when message is queued sucesfully
 * - errno on error
 */
int32_t log_info(const char *message, ...) {

    if (!iIsInitDone) {
        return -1;
    }

    if (iCurrLogLevel < eINFO) {
        return -2;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eINFO, message, args);
    va_end(args);
    return ret;
}

/* Write message on this level
 *
 * Return
 * - -1 when logging system is no init yet
 * - -2 when current level is not sufficient to queue the message
 * - 0 when message is queued sucesfully
 * - errno on error
 */
int32_t log_warning(const char *message, ...) {

    if (!iIsInitDone) {
        return -1;
    }

    if (iCurrLogLevel < eWARNING) {
        return -2;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eWARNING, message, args);
    va_end(args);
    return ret;
}

/* Write message on this level
 *
 * Return
 * - -1 when logging system is no init yet
 * - -2 when current level is not sufficient to queue the message
 * - 0 when message is queued sucesfully
 * - errno on error
 */
int32_t log_error(const char *message, ...) {

    if (!iIsInitDone) {
        return -1;
    }

    if (iCurrLogLevel < eERROR) {
        return -2;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eERROR, message, args);
    va_end(args);
    return ret;
}

/* Force a blocking flush of the current message queue to the datafile
 * Return: Always 0
 */
int32_t logger_flush(void) {
    /* Force flush queue */
    iDoForceFlush = 1;
    while (iDataInQueueCount) {
        usleep(100);
    };
    return 0;
}

/* Init the Logging system
 *
 * Return:
 * -1: Init already done
 * 0: Sucessfull
 * errno on error
 */
int32_t logger_init(const char *pcLogfilePath, tLoggerType Loglevel) {

    if (iIsInitDone) {
        return -1;
    }

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    iCurrLogLevel = Loglevel;

    /* Test access, and create datafile */
    FILE *fd;
    if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
        sdsfree(gpcLogfile);
        return errno;
    }
    fclose(fd);

    /* Spin up writing thread */
    if (pthread_create(&LoggerThread, NULL, logger_thread, NULL) != 0) {
        sdsfree(gpcLogfile);
        return errno;
    }

    /* Setup queue */
    STAILQ_INIT(&Head);
    iDataInQueueCount = 0;

    iDoForceFlush = 0;
    iIsInitDone = 1;

    return 0;
}

/* Destroy the logging system
 *
 * Return:
 * 0: success
 * -1: Already destroyed
 */
int32_t logger_destroy(void) {

    if (!iIsInitDone) {
        return -1;
    }

    log_debug("Destroying the logger.");
    logger_flush();

    /* End logger thread */
    pthread_cancel(LoggerThread);
    pthread_join(LoggerThread, NULL);

    if (gpcLogfile != NULL) {
        sdsfree(gpcLogfile);
        gpcLogfile = NULL;
    }

    /* Not needed, still for safety */
    pthread_mutex_unlock(&pLogMutex);

    /* TailQ deletion */
    tsEntry *n1, *n2;
    n1 = STAILQ_FIRST(&Head);
    while (n1 != NULL) {
        n2 = STAILQ_NEXT(n1, entries);
        sdsfree(n1->msg);   //sds msg
        free(n1);   // queue item
        n1 = n2;
    }

    iDataInQueueCount = 0;

    /* May be init again */
    iIsInitDone = 0;

    return 0;
}


