#ifndef CEWSERVER_CEW_CLIENT_H
#define CEWSERVER_CEW_CLIENT_H

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>

//in seconds, float
#define CLIENT_TIMEOUT 5.

int32_t client_register_ev(int32_t);

#endif //CEWSERVER_CEW_CLIENT_H
