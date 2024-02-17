#ifndef CEWSERVER_CEW_SOCKET_H
#define CEWSERVER_CEW_SOCKET_H

#define BACKLOG 10

#define SOCK_EXIT_SUCCESS 0
#define SOCK_FAIL -1


int32_t socket_setup(char *);

int32_t socket_close(void);

#endif //CEWSERVER_CEW_SOCKET_H
