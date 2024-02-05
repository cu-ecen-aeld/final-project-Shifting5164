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
static FILE *gLogfile = NULL;
static pthread_t LoggerThread;
static pthread_mutex_t pLogMutex = PTHREAD_MUTEX_INITIALIZER;

/* mapping from enum to text */
static const char *pcLogTypeMsg[] = {
        "Debug",
        "Info",
        "Warning",
        "Error"
};

//
//static int32_t log_write(sDataFile *psDataFile, const void *cvpBuff, const int32_t ciSize) {
//
//    int32_t iRet = RET_OK;
//
//    if (pthread_mutex_lock(&psDataFile->pMutex) != 0) {
//        return errno;
//    }
//
//    if ((psDataFile->pFile = fopen(psDataFile->pcFilePath, "a+")) == NULL) {
//        iRet = errno;
//        goto exit_no_open;
//    }
//
//    /* Append received data */
//    fwrite(cvpBuff, ciSize, 1, psDataFile->pFile);
//    if (ferror(psDataFile->pFile) != 0) {
//        iRet = errno;
//    }
//
//    fclose(sGlobalDataFile.pFile);
//
//    exit_no_open:
//    pthread_mutex_unlock(&psDataFile->pMutex);
//
//    return iRet;
//}

static void *logger_thread(void *arg) {

    while (1) {

        if (pthread_mutex_lock(&pLogMutex) != 0) {
            return errno;
        }

        // GET
        // free
        sleep(10);


        if (pthread_mutex_unlock(&pLogMutex) != 0) {
            return errno;
        }

        //write


    }

}


static int32_t log_add(const tLoggerType eType, const char *pcMsg, va_list args) {

    char acTime[64];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(acTime, sizeof(acTime), "%a, %d %b %Y %T %z", tmp);

    char cUserMsg[LOGGER_MAX_USER_MSG_LEN] = {0};
    vsnprintf(cUserMsg, sizeof(cUserMsg), pcMsg, args);

    sds LogMsg = sdscatprintf(sdsempty(), "%s : %s : %s\n", acTime, pcLogTypeMsg[eType], cUserMsg);

    printf("%s", LogMsg);
    sdsfree(LogMsg);

    if (pthread_mutex_lock(&pLogMutex) != 0) {
        return errno;
    }

    // ADD to queue


    if (pthread_mutex_unlock(&pLogMutex) != 0) {
        return errno;
    }

    return 0;

}

int32_t log_error(const char *message, ...) {
    va_list args;
    va_start(args, message);
    log_add(eERROR, message, args);
    va_end(args);
}

int32_t logger_init(const char *pcLogfilePath) {

    /* Set logfile */
    gpcLogfile = sdsnew(pcLogfilePath);

    /* Test access, and create */
    if ((gLogfile = fopen(gpcLogfile, "a+")) == NULL) {
        return errno;
    }
    fclose(gLogfile);

    /* Spin up thread */
    if (pthread_create(&LoggerThread, NULL, logger_thread, NULL) != 0) {
        return errno;
    }

    return 0;
}

int32_t logger_destroy(void) {

    /* End support threads */
    pthread_cancel(LoggerThread);
    pthread_join(LoggerThread, NULL);

    pthread_mutex_unlock(&pLogMutex);

    if (gLogfile != NULL) {
        fclose(gLogfile);
    }

    if (gpcLogfile != NULL) {
        sdsfree(gpcLogfile);
    }

    return 0;
}


