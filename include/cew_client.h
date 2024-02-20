#ifndef CEWSERVER_CEW_CLIENT_H
#define CEWSERVER_CEW_CLIENT_H

#include <stdint.h>

typedef struct sClientStruct{
    int32_t iId;    // unique random id
    // last data for timeout
    // ip
    // socket
}tsClientStruct;

void client_stub(void);

//
///* socket send and recv buffers, per client */
//#define RECV_BUFF_SIZE 1024     /* bytes */
//#define SEND_BUFF_SIZE 1024     /* bytes */
//
///* Connected client info */
//typedef struct sClient {
//    int32_t iSockfd;         /* socket clients connected on */
//    bool bIsDone;             /* client disconnected  ? */
//    struct sockaddr_storage sTheirAddr;
//    socklen_t tAddrSize;
//    char acSendBuff[SEND_BUFF_SIZE];    /* data sending */
//    char acRecvBuff[RECV_BUFF_SIZE];    /* data reception */
//    int32_t iReceived;
//    long lID;               /* random unique id of the thread*/
//} sClient;
//

#endif //CEWSERVER_CEW_CLIENT_H
