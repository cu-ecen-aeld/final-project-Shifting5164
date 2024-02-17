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
#include <banned.h>
#include <cew_logger.h>

static sds gpcLogfile = NULL;
static pthread_t LoggerThread;
static pthread_mutex_t pLogMutex = PTHREAD_MUTEX_INITIALIZER;

/* Default settings for logging */
static tsLogSettings sCurrLogSettings = {
        .iPollingInterval = 1000,
        .iLoggerQueueSize = 50,
        .iBulkWrite = 0, //streaming
        .iCurrLogLevel = eWARNING,
};

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

// Set when logging system has been set up
static int32_t iIsInitDone = 0;

// Force a flush of the message queue to the datafile
static int32_t iDoForceFlush = 0;

/*  Log writing thread.
 *
 * Will try to write the queued logging messages in bulk defined by sCurrLogSettings.iBulkWrite.
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
        usleep(sCurrLogSettings.iPollingInterval);
        pthread_testcancel();

        if ((iDataInQueueCount > sCurrLogSettings.iBulkWrite) || (iDoForceFlush && iDataInQueueCount)) {

            /* Open file for writing */
            if ((fd = fopen(gpcLogfile, "a")) == NULL) {
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
 * Return:
 * - LOG_EXIT_FAILURE, errno will be set. Probably no log message added to queue, and discarded.
 * - LOG_EXIT_SUCCESS, when all good.
 *
 */
static int32_t log_add_to_queue(const tLoggerType eType, const char *pcMsg, va_list args) {

    /* Check max allowed messages in the queue against runaway */
    if (iDataInQueueCount > sCurrLogSettings.iLoggerQueueSize) {
        errno = ENOBUFS;
        return LOG_EXIT_FAILURE;
    }

    /* Add timestamp, IEC compatible */
    time_t t = time(NULL);
    tzset();    // needed for localtime_r
    struct tm local_tm ;
    struct tm *tz = localtime_r(&t, &local_tm);
    char acTime[64];
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tz);

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
            int32_t tmperr = errno;

            sdsfree(LogMsg);
            free(NewEntry);

            errno = tmperr;
            return LOG_EXIT_FAILURE;
        }

        /* Push at the tail, using head for popping */
        STAILQ_INSERT_TAIL(&Head, NewEntry, entries);
        iDataInQueueCount++;

        if (pthread_mutex_unlock(&pLogMutex) != 0) {
            return LOG_EXIT_FAILURE;
        }
    }

    return LOG_EXIT_SUCCESS;
}


static inline int32_t is_msg_allowed(tLoggerType eType) {
    if (!iIsInitDone) {
        return LOG_NOLVL;
    }

    if (sCurrLogSettings.iCurrLogLevel < eType) {
        return LOG_NOINIT;
    }

    return LOG_EXIT_SUCCESS;
}


/* Write message on this level
 *
 * Return
 * - LOG_NOLVL when logging system is no init yet
 * - LOG_NOINIT when current level is not sufficient to queue the message
 * - LOG_LOG_EXIT_SUCCESS when message is queued sucesfully
 * - LOG_EXIT_FAILURE, no error
 */
int32_t log_msg(tLoggerType eType, const char *message, ...) {

    if (is_msg_allowed(eType)) {
        return LOG_EXIT_FAILURE;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eType, message, args);
    va_end(args);

    return ret;
}

/* Force a blocking flush of the current message queue to the datafile
 * Return: Always LOG_EXIT_SUCCESS
 */
int32_t logger_flush(void) {
    /* Force flush queue */
    iDoForceFlush = 1;
    while (iDataInQueueCount) {
        usleep(100);
    }

    return LOG_EXIT_SUCCESS;
}

/* Init the Logging system
 *
 * Return:
 * LOG_NOINIT: Init already done
 * 0: Sucessfull
 * errno on error
 */
int32_t logger_init(const char *pcLogfilePath, tLoggerType Loglevel) {

    if (iIsInitDone) {
        return LOG_NOINIT;
    }

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    sCurrLogSettings.iCurrLogLevel = Loglevel;

    /* Test access, and create datafile */
    FILE *fd;
    if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
        sdsfree(gpcLogfile);
        return LOG_EXIT_FAILURE;
    }
    fclose(fd);

    /* Spin up writing thread */
    if (pthread_create(&LoggerThread, NULL, logger_thread, NULL) != 0) {
        sdsfree(gpcLogfile);
        return LOG_EXIT_FAILURE;
    }

    /* Setup queue */
    STAILQ_INIT(&Head);
    iDataInQueueCount = 0;

    iDoForceFlush = 0;
    iIsInitDone = 1;

    return LOG_EXIT_SUCCESS;
}

/* Destroy the logging system
 *
 * Return:
 * 0: success
 * LOG_NOINIT: Already destroyed
 */
int32_t logger_destroy(void) {

    if (!iIsInitDone) {
        return LOG_NOINIT;
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

    return LOG_EXIT_SUCCESS;
}


/* Return only a copy */
tsLogSettings logger_get(void) {
    return sCurrLogSettings;
}

int32_t logger_set(tsLogSettings sNewSettings) {
    sCurrLogSettings = sNewSettings;

    return LOG_EXIT_SUCCESS;
}


