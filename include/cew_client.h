#ifndef CEWSERVER_CEW_CLIENT_H
#define CEWSERVER_CEW_CLIENT_H

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sds.h>

//in seconds, float
#define CLIENT_TIMEOUT 5.

/* socket send and recv buffers, per client */
#define RECV_BUFF_SIZE 4096     /* Bytes */

typedef struct sClientStruct {
    int32_t iId;                            // unique random id
    int32_t iSockfd;         /* socket clients connected on */
    struct sockaddr_in sTheirAddr;
    int8_t cIP[INET_ADDRSTRLEN];
    sds acSendBuff;    /* Data sending */
    char acRecvBuff[RECV_BUFF_SIZE];    /* Data reception */
} tsClientStruct;


int32_t client_register_ev(int32_t);

#endif //CEWSERVER_CEW_CLIENT_H
