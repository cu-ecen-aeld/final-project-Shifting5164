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
#include <cew_exit.h>

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
STAILQ_HEAD(LogMsgQueue, sQEntry);
static struct LogMsgQueue Head;

// Logfile
static FILE *LogfileFd = NULL;

// Amount of messages in the queue
static int32_t iDataInQueueCount = 0;       //mutex safe pLogMutex

// Set when logging system has been set up
static int32_t iIsInitDone = 0;

// Force a flush of the message queue to the datafile
static int32_t iDoForceFlush = 0;

static int32_t iShuttingDown = 0;

/*  Log writing thread.
 *
 * Will try to write the queued logging messages in bulk defined by sCurrLogSettings.iBulkWrite.
 *
 * Args: None
 * Return: errno
 */
static void *logger_thread(void *arg) {

    int32_t iOldCancelState;
    int32_t iDataToWrite;
    int32_t iDataWritten;

    while (1) {

        /* Thread cancelation block. The thread is only allowed to cancel here. Byond this point
         * dynamic memory safety is a problem, cleanup function will be complex, so
         * allow the thread to only cancel here, where we know its alwasy safe to do
         */
        {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &iOldCancelState);
            pthread_testcancel();
            usleep(sCurrLogSettings.iPollingInterval);
            pthread_testcancel();
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iOldCancelState);
        }

        int32_t iGo = 0;
        /* MUTEX block for iDataInQueueCount */
        {
            int32_t iRet;
            if ((iRet = pthread_mutex_trylock(&pLogMutex)) != 0) {
                if (iRet == EBUSY) {
                    continue;
                }
                exit(errno);
            }

            if ((iDataInQueueCount > sCurrLogSettings.iBulkWrite) || (iDoForceFlush && iDataInQueueCount)) {
                iGo = 1;
                iDataToWrite = iDataInQueueCount;
                iDataWritten = 0;
            }

            if (pthread_mutex_unlock(&pLogMutex) != 0) {
                exit(errno);
            }
        }

        /* If we have a go, then we are going to write the logs in the queue defined by iDataToWrite.
         * When we written everything, then we will update the message queue waiting size.*/
        if (iGo) {

            /* Open logfile for writing */
            if ((LogfileFd = fopen(gpcLogfile, "a")) == NULL) {
                exit(errno);
            }

            /* Bulk write */
            do {

                /* Get data from head, save the log message pointer for later use */
                sds LogMessage;
                tsEntry *Entry = STAILQ_FIRST(&Head);
                LogMessage = Entry->msg;

                /* mutex block, only for removing the queue head entry */
                {
                    if (pthread_mutex_lock(&pLogMutex) != 0) {
                        exit(errno);
                    }

                    /* Delete entry from the head */
                    STAILQ_REMOVE_HEAD(&Head, entries);
                    free(Entry);

                    if (pthread_mutex_unlock(&pLogMutex) != 0) {
                        exit(errno);
                    }
                }

#ifdef LOGGER_DEBUG_SHOW_ON_TERMINAL
                /* Show log message of stderr */
                fprintf(stderr, "%s", LogMessage);
#endif
                /* Write the log entry to the file */
                fwrite(LogMessage, sdslen(LogMessage), 1, LogfileFd);
                if (ferror(LogfileFd) != 0) {
                    exit(errno);
                }

                /* Free the allocated message */
                sdsfree(LogMessage);

                /* keep track of the actual written data */
                iDataWritten++;

            } while (--iDataToWrite);

            /* Flush and close file after bulk write */
            if (fflush(LogfileFd) != 0) {
                exit(errno);
            }

            if (fclose(LogfileFd) != 0) {
                exit(errno);
            }
            LogfileFd = NULL;

            /* MUTEX block to update iDataInQueueCount */
            {
                if (pthread_mutex_lock(&pLogMutex) != 0) {
                    exit(errno);
                }

                /* Make sure we only flush once when set */
                iDoForceFlush = 0;

                /* Update total message count */
                iDataInQueueCount -= iDataWritten;

                if (pthread_mutex_unlock(&pLogMutex) != 0) {
                    exit(errno);
                }
            }
        }
    }
}

/* Add a log message to the writing queue. Writing will be outsourced to a writing thread.
 *
 * Return:
 * - LOG_EXIT_FAILURE, errno will be set. Probably no log message added to queue, and discarded.
 * - LOG_EXIT_SUCCESS, when all good.

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
    struct tm local_tm;
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

/* Determine if a message is allowed in the logger block or not */
static inline int32_t is_msg_allowed(tLoggerType eType) {

    /* Without init no msg */
    if (!iIsInitDone) {
        return LOG_NOLVL;
    }

    /* If the program is shutting down, no msg accepted */
    if (bTerminateProg) {
        return LOG_SHUTTING_DOWN;
    }

    /* If the logger module is shutting down, no msg accepted */
    if (iShuttingDown) {
        return LOG_SHUTTING_DOWN;
    }

    /* Check current loglevel */
    if (sCurrLogSettings.iCurrLogLevel < eType) {
        return LOG_NOINIT;
    }

    return LOG_EXIT_SUCCESS;
}

#ifdef LOGGER_DEBUG_SHOW_ON_TERMINAL_NO_QUEUE

static void log_msg_stdout(const tLoggerType eType, const char *pcMsg, va_list args) {

    /* Add timestamp, IEC compatible */
    time_t t = time(NULL);
    tzset();    // needed for localtime_r
    struct tm local_tm;
    struct tm *tz = localtime_r(&t, &local_tm);
    char acTime[64];
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tz);

    /* Parse the users message */
    char cUserMsg[LOGGER_MAX_USER_MSG_LEN] = {0};
    vsnprintf(cUserMsg, sizeof(cUserMsg), pcMsg, args);

    fprintf(stdout,"%s\n", cUserMsg);
}

#endif

/* Write message on this level
 *
 * Return
 * - LOG_NOLVL when logging system is no init yet
 * - LOG_NOINIT when current level is not sufficient to queue the message
 * - LOG_LOG_EXIT_SUCCESS when message is queued sucesfully
 * - LOG_EXIT_FAILURE, no error
 */
int32_t log_msg(tLoggerType eType, const char *message, ...) {

    int32_t iRet;

    va_list(args);
    va_start(args, message);

#ifdef LOGGER_DEBUG_SHOW_ON_TERMINAL_NO_QUEUE
    log_msg_stdout(eType, message, args);
#endif

    if (is_msg_allowed(eType) == LOG_EXIT_SUCCESS) {
        iRet = log_add_to_queue(eType, message, args);
    } else {
        iRet = LOG_EXIT_FAILURE;
    }

    va_end(args);

    return iRet;
}


/* Force a blocking flush of the current message queue to the datafile
 * Return: Always LOG_EXIT_SUCCESS
 */
int32_t logger_flush(void) {

    /* Force flush queue */
    iDoForceFlush = 1;

    /* Prevent blocking, when nothing got flushed after this then just lose the data
     * NOTE: when we fork the thread doesn't run, so don't block.
     * */
    int32_t MaxLoops = 20;
    while (MaxLoops--) {
        if (!iDataInQueueCount) {
            break;
        }
        usleep(500);
    }

    printf("DONE\n");

    return LOG_EXIT_SUCCESS;
}

/* Init the Logging system
 *
 * Return:
 * LOG_NOINIT: Init already done
 * 0: Successful
 * errno on error
 */
int32_t logger_init(const char *pcLogfilePath, tLoggerType Loglevel) {

    if (iIsInitDone) {
        return LOG_NOINIT;
    }

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    /* Set loglevel */
    sCurrLogSettings.iCurrLogLevel = Loglevel;

    /* Test access, and create datafile */
    FILE *fd;
    if ((fd = fopen(gpcLogfile, "a+")) == NULL) {
        sdsfree(gpcLogfile);
        return LOG_EXIT_FAILURE;
    }
    fclose(fd);

    /* Setup queue */
    STAILQ_INIT(&Head);
    iDataInQueueCount = 0;

    iDoForceFlush = 0;
    iShuttingDown = 0;

    /* Spin up writing thread */
    if (pthread_create(&LoggerThread, NULL, logger_thread, NULL) != 0) {
        sdsfree(gpcLogfile);
        return LOG_EXIT_FAILURE;
    }

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

    log_debug("Destroying the logger from pid %d.", getpid());
    iShuttingDown = 1;
    logger_flush();

    /* End logger thread */
    pthread_cancel(LoggerThread);
    pthread_join(LoggerThread, NULL);

    /* We don't know if everything got flushed and closed in the logger */
    if (LogfileFd != NULL) {
        fclose(LogfileFd);
        LogfileFd = NULL;
    }

    if (gpcLogfile != NULL) {
        sdsfree(gpcLogfile);
        gpcLogfile = NULL;
    }

    /* Not needed, still for safety */
    pthread_mutex_unlock(&pLogMutex);

    /* TailQ deletion, if there is still data left */
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


