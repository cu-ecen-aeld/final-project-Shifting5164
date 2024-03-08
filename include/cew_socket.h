#ifndef CEWSERVER_CEW_SOCKET_H
#define CEWSERVER_CEW_SOCKET_H

#include <stdint.h>
#include <ev.h>
#include <cew_client.h>

#define BACKLOG 10

#define SOCK_EXIT_SUCCESS 0
#define SOCK_FAIL -1


/* Open socket on specified port */
int32_t socket_setup(uint16_t, int32_t *);

/* Socket accept, fills client info for worker, no handling */
int32_t socket_poll(void);

/* Close socket */
int32_t socket_close(void);

void socket_accept_client(struct ev_loop *loop, ev_io *w_, int revents);

#endif //CEWSERVER_CEW_SOCKET_H
