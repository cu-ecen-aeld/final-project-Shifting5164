#ifndef CEWSERVER_LOGGER_H
#define CEWSERVER_LOGGER_H

#include <sds.h>

// Maximum queue size
#define LOGGER_QUEUE_SIZE 50

// Amount of messages to be in the queue to start writing in bulk, 0 = streaming
#define BULK_WRITE 0

// Max characters in logging msg
#define LOGGER_MAX_USER_MSG_LEN 1024

typedef enum {
    eERROR = 0,
    eWARNING,
    eINFO,
    eDEBUG,
} tLoggerType;

int32_t log_debug(const char *, ...);

int32_t log_info(const char *, ...);

int32_t log_warning(const char *, ...);

int32_t log_error(const char *, ...);


int32_t logger_flush(void);

int32_t logger_init(const char *, tLoggerType);

int32_t logger_destroy(void);


#endif //CEWSERVER_LOGGER_H
