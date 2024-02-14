#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>
#include "logger.h"

static sds gpcLogfile = NULL;
static pthread_t LoggerThread;
static pthread_mutex_t pLogMutex = PTHREAD_MUTEX_INITIALIZER;

/* mapping from enum to text */
static const char *pcLogTypeMsg[] = {
        "Debug",
        "Info",
        "Warning",
        "Error"
};

typedef struct sEntry {
    sds data;
    STAILQ_ENTRY(entry) entries;        /* Singly linked tail queue */
} tsEntry;

STAILQ_HEAD(stailhead, sEntry);
static struct stailhead Head;
static int32_t iDataInQueue = 0;
static int32_t iInitDone = 0;
static int32_t iForceFlush = 0;
static int32_t iLoglevel = eWARNING;

static int32_t logger_thread(void *arg) {

    while (1) {

        usleep(100);

        if ((iDataInQueue > BULK_WRITE) || (iForceFlush && iDataInQueue)) {

            /* Open file for writing */
            FILE *fd = NULL;
            if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
                return errno;
            }

            /* Bulk write */
            do {

                // mutex block
                sds LogMessage;
                {
                    if (pthread_mutex_lock(&pLogMutex) != 0) {
                        return errno;
                    }

                    /* Get data */
                    tsEntry *Entry = STAILQ_FIRST(&Head);
                    LogMessage = Entry->data;

                    /* Deletion from the head */
                    STAILQ_REMOVE_HEAD(&Head, entries);
                    free(Entry);
                    iDataInQueue--;

                    if (pthread_mutex_unlock(&pLogMutex) != 0) {
                        return errno;
                    }
                }

                /* Write the entry */
                fwrite(LogMessage, sdslen(LogMessage), 1, fd);
                if (ferror(fd) != 0) {
                    return errno;
                }

                // free
                sdsfree(LogMessage);

            } while (iDataInQueue);

            iForceFlush = 0;

            /* Flush and close after bulk write */
            if (fflush(fd) != 0) {
                return errno;
            }

            if (fclose(fd) != 0) {
                return errno;
            }
        }
    }
}

static int32_t log_add_to_queue(const tLoggerType eType, const char *pcMsg, va_list args) {

    /* Check space against runaway */
    if (iDataInQueue > LOGGER_QUEUE_SIZE) {
        return -1;
    }

    char acTime[64];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tmp);

    char cUserMsg[LOGGER_MAX_USER_MSG_LEN] = {0};
    vsnprintf(cUserMsg, sizeof(cUserMsg), pcMsg, args);

    /* Make the log entry
     * free @ logger_thread */
    sds LogMsg = sdscatprintf(sdsempty(), "%s : %s : %s", acTime, pcLogTypeMsg[eType], cUserMsg);

    /* Pack message in queue entry
     * free @ logger_thread
     * */
    tsEntry *NewEntry = malloc(sizeof(tsEntry));
    NewEntry->data = LogMsg;

    if (pthread_mutex_lock(&pLogMutex) != 0) {
        return errno;
    }

    /* Insert at the tail */
    STAILQ_INSERT_TAIL(&Head, NewEntry, entries);
    iDataInQueue++;

    if (pthread_mutex_unlock(&pLogMutex) != 0) {
        return errno;
    }

    return 0;
}

int32_t log_debug(const char *message, ...) {

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eDEBUG){
        return 0;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eDEBUG, message, args);
    va_end(args);
    return ret;
}

int32_t log_info(const char *message, ...) {

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eINFO){
        return 0;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eINFO, message, args);
    va_end(args);
    return ret;
}

int32_t log_warning(const char *message, ...) {

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eWARNING){
        return 0;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eWARNING, message, args);
    va_end(args);
    return ret;
}

int32_t log_error(const char *message, ...) {

    if (!iInitDone) {
        return -1;
    }

    if (iLoglevel < eERROR){
        return 0;
    }

    va_list args;
    va_start(args, message);
    int32_t ret = log_add_to_queue(eERROR, message, args);
    va_end(args);
    return ret;
}

/* Blocking flush */
int32_t logger_flush(void) {
    /* Force flush queue */
    iForceFlush = 1;
    while (iDataInQueue);
    return 0;
}

int32_t logger_init(const char *pcLogfilePath, tLoggerType Loglevel) {

    if (iInitDone) {
        return -1;
    }

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    iLoglevel = Loglevel;

    /* Test access, and create */
    FILE *fd;
    if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
        sdsfree(gpcLogfile);
        return errno;
    }
    fclose(fd);

    /* Spin up thread */
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

int32_t logger_destroy(void) {

    if (!iInitDone) {
        return -1;
    }

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


