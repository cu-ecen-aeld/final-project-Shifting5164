#ifndef CEWSERVER_LOGGER_H
#define CEWSERVER_LOGGER_H

#include <sds.h>

#define LOGGER_QUEUE_SIZE 50    // maximum queue size
#define BULK_WRITE 0   // amount of messages minimum in the queue to start writing, 0 = streaming
#define LOGGER_MAX_USER_MSG_LEN 1024    //max characters in logging msg

typedef enum {
    eDEBUG = 0,
    eINFO,
    eWARNING,
    eERROR,
} tLoggerType;

int32_t log_debug(const char *, ...);

int32_t log_info(const char *, ...);

int32_t log_warning(const char *, ...);

int32_t log_error(const char *, ...);


int32_t logger_flush(void);

int32_t logger_init(const char *, tLoggerType);

int32_t logger_destroy(void);


#endif //CEWSERVER_LOGGER_H