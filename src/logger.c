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

#include "logger.h"

static sds gpcLogfile = NULL;
static pthread_t LoggerThread;
static pthread_mutex_t pLogMutex = PTHREAD_MUTEX_INITIALIZER;

/* mapping from enum to text */
static const char *pcLogTypeMsg[] = {
        "Error",
        "Warning",
        "Info",
        "Debug",
};

typedef struct sEntry {
    sds data;
    STAILQ_ENTRY(sEntry) entries;        /* Singly linked tail queue */
} tsEntry;

STAILQ_HEAD(stailhead, sEntry);
static struct stailhead Head;
static int32_t iDataInQueue = 0;
static int32_t iInitDone = 0;
static int32_t iForceFlush = 0;
static int32_t iLoglevel = eWARNING;

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

        /* Thread may cancel here, no mallocs or open fd */
        pthread_testcancel();

        usleep(100); // TODO EV

        if ((iDataInQueue > BULK_WRITE) || (iForceFlush && iDataInQueue)) {

            /* Open file for writing */
            if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
                exit(errno);
            }

            /* Bulk write */
            do {

                /* Get data from head, save the log message pointer for later use */
                sds LogMessage;
                tsEntry *Entry = STAILQ_FIRST(&Head);
                LogMessage = Entry->data;

                // mutex block
                {
                    if (pthread_mutex_lock(&pLogMutex) != 0) {
                        exit(errno);
                    }

                    /* Delete entry from the head */
                    STAILQ_REMOVE_HEAD(&Head, entries);
                    free(Entry);
                    iDataInQueue--;

                    if (pthread_mutex_unlock(&pLogMutex) != 0) {
                        exit(errno);
                    }
                }

                /* Write the log entry */
                fwrite(LogMessage, sdslen(LogMessage), 1, fd);
                if (ferror(fd) != 0) {
                    exit(errno);
                }

                // free
                sdsfree(LogMessage);

            } while (iDataInQueue);

            iForceFlush = 0;

            /* Flush and close after bulk write */
            if (fflush(fd) != 0) {
                exit(errno);
            }

            if (fclose(fd) != 0) {
                exit(errno);
            }
        }
    }

    exit(0);
}

/* Add a log message to the writing queue. Writing will be outsourced to a writing thread.
 *
 * Return: errno on error, else 0
 */
static int32_t log_add_to_queue(const tLoggerType eType, const char *pcMsg, va_list args) {

    /* Check space against runaway */
    if (iDataInQueue > LOGGER_QUEUE_SIZE) {
        return ENOBUFS;
    }

    char acTime[64];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tmp);

    char cUserMsg[LOGGER_MAX_USER_MSG_LEN] = {0};
    vsnprintf(cUserMsg, sizeof(cUserMsg), pcMsg, args);

    /* Make the log entry
     * free @ logger_thread */
    sds LogMsg = sdscatprintf(sdsempty(), "%s : %s : %s\n", acTime, pcLogTypeMsg[eType], cUserMsg);

    /* Pack message in queue entry
     * free @ logger_thread
     * */
    tsEntry *NewEntry = malloc(sizeof(tsEntry));
    NewEntry->data = LogMsg;

    // mutex safe block
    {
        if (pthread_mutex_lock(&pLogMutex) != 0) {
            free(NewEntry);
            return errno;
        }

        /* Insert at the tail */
        STAILQ_INSERT_TAIL(&Head, NewEntry, entries);
        iDataInQueue++;

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

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eDEBUG) {
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

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eINFO) {
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

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eWARNING) {
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

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eERROR) {
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
    iForceFlush = 1;
    while (iDataInQueue) {
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

    if (iInitDone) {
        return -1;
    }

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    iLoglevel = Loglevel;

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
    iDataInQueue = 0;

    iInitDone = 1;
    iForceFlush = 0;

    return 0;
}

/* Destroy the logging system
 *
 * Return:
 * 0: success
 * -1: Already destroyed
 */
int32_t logger_destroy(void) {

    if (!iInitDone) {
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

    pthread_mutex_unlock(&pLogMutex);

    /* TailQ deletion */
    tsEntry *n1, *n2;
    n1 = STAILQ_FIRST(&Head);
    while (n1 != NULL) {
        n2 = STAILQ_NEXT(n1, entries);
        sdsfree(n1->data);
        free(n1);
        n1 = n2;
    }

    iDataInQueue = 0;

    /* May be init again */
    iInitDone = 0;

    return 0;
}


