#ifndef CEWSERVER_CEW_LOGGER_H
#define CEWSERVER_CEW_LOGGER_H

#include <stdint.h>

// Max characters in logging msg
#define LOGGER_MAX_USER_MSG_LEN 1024

// Loglevels
typedef enum {
    eERROR = 0,
    eWARNING,
    eINFO,
    eDEBUG,
} tLoggerType;


/* Logger settings */
typedef struct sLogSettings {
    int32_t iPollingInterval;   // usleep
    int32_t iLoggerQueueSize;   // Maximum queue entries
    int32_t iBulkWrite;         // Amount of messages to be in the queue before its writing in bulk, 0 = streaming
    int32_t iCurrLogLevel;      // Dynamic logging level
} tsLogSettings;

tsLogSettings sCurrLogSettings = {
        .iPollingInterval = 100,    // usleep
        .iLoggerQueueSize = 50,
        .iBulkWrite = 10,
        .iCurrLogLevel = eWARNING,
};

/* Return definitions */
#define LOG_EXIT_SUCCESS EXIT_SUCCESS
#define LOG_EXIT_FAILURE EXIT_FAILURE   // + errno usually
#define LOG_NOINIT (-1)
#define LOG_NOLVL (-2)


/* Log types */
int32_t log_msg(tLoggerType, const char *, ...);

#define log_debug(...) log_msg(eDEBUG,__VA_ARGS__)
#define log_info(...) log_msg(eINFO,__VA_ARGS__)
#define log_warning(...) log_msg(eWARNING,__VA_ARGS__)
#define log_error(...) log_msg(eERROR,__VA_ARGS__)


int32_t logger_init(const char *, tLoggerType);

int32_t logger_destroy(void);

/* Force blocking flush of the queue to the datafile */
int32_t logger_flush(void);

#endif //CEWSERVER_CEW_LOGGER_H
