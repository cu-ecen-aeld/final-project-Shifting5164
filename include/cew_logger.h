/* Logger framework. Threadsafe logging implementation to a datafile.
 *
 * Options:
 * - Bulk and streaming writing.
 * - Force flushing of message queue.
 * - Multiple logging levels.
 * - Definable queue size.
 * - Definable polling time.
 *
 * Principle of operation :
 * 1) User has choice of several logging levels.
 * 2) Log messages are created on the heap, the pointer will be added to a tail queue.
 * 3) A separate thread will write the messages from the queue in a bulk or streaming manner.
 * 4) Writing will be undertaken when the polling period expires and data is available.
 *
 * Why STAILQ: 
 * From the man: https://www.man7.org/linux/man-pages/man3/stailq.3.html
 * This structure contains a pair of pointers, one to the first element in the tail
 * queue and the other to the last element in the tail queue.  The elements are
 * singly linked for minimum space and pointer manipulation overhead at the expense
 * of O(n) removal for arbitrary elements.
 *
 * The design uses only the head and tail, thus an O(1) system.
 *
 */

#ifndef CEWSERVER_CEW_LOGGER_H
#define CEWSERVER_CEW_LOGGER_H

#include <stdint.h>

// Max characters in logging msg
#define LOGGER_MAX_USER_MSG_LEN 1024

// Show messages on terminal on stdout. Good for debugging. Only works good when iBulkWrite=0
#define LOGGER_SHOW_ON_TERMINAL

// Loglevels
typedef enum {
    eERROR = 0,
    eWARNING,
    eINFO,
    eDEBUG,
} tLoggerType;

/* Logger settings */
typedef struct sLogSettings {
    int32_t iPollingInterval;       // usleep
    int32_t iLoggerQueueSize;       // Maximum queue entries
    int32_t iBulkWrite;             // Amount of messages to be in the queue before its writing in bulk, 0 = streaming
    tLoggerType iCurrLogLevel;      // Dynamic logging level
} tsLogSettings;

/* Return definitions */
#define LOG_EXIT_SUCCESS EXIT_SUCCESS
#define LOG_EXIT_FAILURE EXIT_FAILURE   // + errno usually
#define LOG_NOINIT (-1)
#define LOG_NOLVL (-2)
#define LOG_SHUTTING_DOWN (-4)


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

/* Get a copy of the settings */
tsLogSettings logger_get(void);

/* Set new settings */
int32_t logger_set(tsLogSettings);

#endif //CEWSERVER_CEW_LOGGER_H
