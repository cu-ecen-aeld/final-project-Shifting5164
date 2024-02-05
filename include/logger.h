#ifndef CEWSERVER_LOGGER_H
#define CEWSERVER_LOGGER_H

#include <sds.h>

#define LOGGER_QUEUE_SIZE 50
#define LOGGER_MAX_USER_MSG_LEN 1024    //max characters in logging msg

typedef enum {
    eDEBUG =0,
    eINFO,
    eWARNING,
    eERROR,
} tLoggerType;

int32_t log_error(const char* , ...);
int32_t logger_init(const char*);
int32_t logger_destroy(void);


#endif //CEWSERVER_LOGGER_H