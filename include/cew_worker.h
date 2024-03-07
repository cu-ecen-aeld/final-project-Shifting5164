#ifndef CEWSERVER_CEW_WORKER_H
#define CEWSERVER_CEW_WORKER_H

#include <stdlib.h>

#include <cew_logger.h>
#include <cew_client.h>

/* The config will define the current workers and clients dynamically
 * but to make everything faster move some things to compile time, so
 * we will have fixed locations and sizes. If a system needs more then the
 * currently defined setttings, just bump.
 *
 * This is a tradeoff between dynamic and static, speed vs space. Chosen speed.
 */
#define WORKER_MAX_WORKERS 64       // this means 64 threads / cores
#define WORKER_MAX_CLIENTS 512      // 64*512 = 27648 concurrent clients. Should be enough.

/* Return definitions */
#define WORKER_EXIT_SUCCESS EXIT_SUCCESS
#define WORKER_EXIT_FAILURE EXIT_FAILURE   // + errno usually

#define WORKER_IPC_FOLDER "/run/cewserver"
#define WORKER_IPC_FILE "cew_worker"     // + some random number, it will look like cew_worker_%d

int32_t worker_init(int32_t);

int32_t worker_destroy(void);

int32_t worker_route_client(tsClientStruct *);

_Noreturn void worker_monitor(void);
_Noreturn void worker_dummy_send(void);

#endif //CEWSERVER_CEW_WORKER_H
