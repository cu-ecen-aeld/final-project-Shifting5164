#ifndef CEWSERVER_CEW_CLIENT_H
#define CEWSERVER_CEW_CLIENT_H

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>

///* socket send and recv buffers, per client */
//todo dynamic..
#define RECV_BUFF_SIZE 1024     /* bytes */
#define SEND_BUFF_SIZE 1024     /* bytes */

typedef struct sClientStruct {
    int32_t iId;                            // unique random id
    int32_t iSockfd;         /* socket clients connected on */
    bool bIsDone;             /* client disconnected  ? */
    struct sockaddr_storage sTheirAddr;
    socklen_t tAddrSize;
    char acSendBuff[SEND_BUFF_SIZE];    /* data sending */
    char acRecvBuff[RECV_BUFF_SIZE];    /* data reception */
    int32_t iReceived;
    long lID;               /* random unique id of the thread*/
} tsClientStruct;

/* Create a client, malloc's ** */
int32_t client_init(tsClientStruct **);

/* Destroy a client */
void client_destroy(tsClientStruct *);


#endif //CEWSERVER_CEW_CLIENT_H
