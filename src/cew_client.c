#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <ev.h>

#include <cew_client.h>
#include <cew_logger.h>
#include <cew_worker.h>


/* socket send and recv buffers, per client */
#define RECV_BUFF_SIZE 1024     /* Bytes */
#define SEND_BUFF_SIZE 1024     /* Bytes */

typedef struct sClientStruct {
    int32_t iId;                            // unique random id
    int32_t iSockfd;         /* socket clients connected on */
    bool bIsDone;             /* client disconnected  ? */
    struct sockaddr_storage sTheirAddr;
    socklen_t tAddrSize;
    char acSendBuff[SEND_BUFF_SIZE];    /* Data sending */
    char acRecvBuff[RECV_BUFF_SIZE];    /* Data reception */
    int32_t iReceived;
} tsClientStruct;

/* ev composit block
 * http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#GLOBAL_FUNCTIONS
 * BUILDING YOUR OWN COMPOSITE WATCHERS
 */
typedef struct sClientEv {
    tsClientStruct sClient;
    ev_io io;
    ev_timer timer;
} tsClientEv;


//
//static test_send(){
//
//    char msg[] = "sending...\n";
//    while (1) {
//        if (send(fd, msg, strlen(msg), 0) == -1) {
//            close(fd);
//            log_debug("Disconnect");
//        }
//
//        sleep(1);
//    }
//}


static int32_t client_deregister(tsClientEv *psClientEv) {

    int32_t iId = psClientEv->sClient.iId;

    ev_timer_stop(psWorkerEVLoop, &psClientEv->timer);
    ev_io_stop(psWorkerEVLoop, &psClientEv->io);

    if (psClientEv->sClient.iSockfd) {
        close(psClientEv->sClient.iSockfd);
        psClientEv->sClient.iSockfd = 0;
    }

    free(psClientEv);

    log_info("Deregistered client %d.", iId);

    return EXIT_SUCCESS;
}

static void client_callback_timeout(struct ev_loop *loop, ev_timer *w, int revents) {
    tsClientEv *psClientEv = (struct sClientEv *) (((uint8_t *) w) - offsetof (struct sClientEv, timer));
    tsClientStruct *psClient = &psClientEv->sClient;

    log_info("Client timeout %d.", psClient->iId);

    client_deregister(psClientEv);
}

static void client_callback_serve(struct ev_loop *loop, ev_io *w, int revents) {
    tsClientEv *psClientEv = (struct sClientEv *) (((uint8_t *) w) - offsetof (struct sClientEv, io));
    tsClientStruct *psClient = &psClientEv->sClient;

    psClient->iReceived = recv(psClient->iSockfd, psClient->acRecvBuff, RECV_BUFF_SIZE, 0);
    if (psClient->iReceived < 0) {
        /* Error */
        log_error("Receive error on client %d.", psClient->iId);
    } else if (psClient->iReceived == 0) {
        /* This is the only way a client can disconnect */
        log_debug("Disconnecting client %d", psClient->iId);
        client_deregister(psClientEv);
    } else if (psClient->iReceived) {
        /* Got data from client, do stuff */

        ev_timer_again(psWorkerEVLoop, &psClientEv->timer);

        snprintf(psClient->acSendBuff, sizeof(psClient->acSendBuff), "id:%d. got data...\n", psClient->iId);
        if (send(psClient->iSockfd, psClient->acSendBuff, strlen(psClient->acSendBuff), 0) == -1) {
            log_debug("Sending error on client %d", psClient->iId);
        }
    }
}

/* Dynamically register a client and callbacks */
int32_t client_register_ev(int32_t iSockfd) {

    tsClientEv *psClientEv = malloc(sizeof(struct sClientEv));
    memset(psClientEv, 0, sizeof(struct sClientEv));

    psClientEv->sClient.iSockfd = iSockfd;
    psClientEv->sClient.iId = (int32_t) random();

    log_info("Added callback for client %d", psClientEv->sClient.iId);

    /* Client receive timeout timer */
    ev_timer_init (&psClientEv->timer, client_callback_timeout, CLIENT_TIMEOUT, CLIENT_TIMEOUT);
    ev_timer_start(psWorkerEVLoop, &psClientEv->timer);

    /* Client serving callback */
    ev_io_init(&psClientEv->io, client_callback_serve, psClientEv->sClient.iSockfd, EV_READ);
    ev_io_start(psWorkerEVLoop, &psClientEv->io);

    return EXIT_SUCCESS;
}


//
//
//
//
//// will malloc psNewClient
////todo return codes
//int32_t client_init(tsClientStruct **ppsNewClient) {
//    *ppsNewClient = malloc(sizeof(struct sClientStruct));
//    memset(*ppsNewClient, 0, sizeof(struct sClientStruct));
//    return 0;
//}
//
//void client_destroy(tsClientStruct *psClient) {
//    if (psClient == NULL) {
//        return;
//    }
//
//    struct stat statbuf = {0};
//    if (fstat(psClient->iSockfd, &statbuf) == -1){
//        if (errno != EBADF) {
//            close(psClient->iSockfd);
//        }
//    }
//
//    free(psClient);
//}



//
//
//static void *client_serve(void *arg) {
//
//    sClient *psClient = (sClient *) arg;
//
//    /* Get IP connecting client */
//    struct sockaddr_in *sin = (struct sockaddr_in *) &psClient->sTheirAddr;
//    unsigned char *ip = (unsigned char *) &sin->sin_addr.s_addr;
//    syslog(LOG_DEBUG, "Accepted connection from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
//
//    /* Keep receiving data until error or disconnect*/
//    int32_t iRet = 0;
//
//    while (1) {
//        psClient->iReceived = recv(psClient->iSockfd, psClient->acRecvBuff, RECV_BUFF_SIZE, 0);
//
//        if (psClient->iReceived < 0) {
//            /* Error */
//            do_thread_exit_with_errno(__LINE__, iRet);
//        } else if (psClient->iReceived == 0) {
//            /* This is the only way a client can disconnect */
//
//            close(psClient->iSockfd);
//            syslog(LOG_DEBUG, "Connection closed from %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
//
//            /* Signal housekeeping */
//            psClient->bIsDone = true;
//
//            pthread_exit((void *) RET_OK);
//
//        } else if (psClient->iReceived) {
//            /* Got data from client, do stuff */
//
//            /* Search for a complete message, determined by the "\n" end character */
//            char *pcEnd = NULL;
//            if ((pcEnd = strstr(psClient->acRecvBuff, "\n")) == NULL) {
//
//                /* not end of message yet, write all we received */
//                if ((iRet = file_write(psClient->psDataFile, psClient->acRecvBuff, psClient->iReceived)) != 0) {
//                    do_thread_exit_with_errno(__LINE__, iRet);
//                }
//
//                continue;
//            }
//
//            /* End of message detected, write until message end */
//
//            // NOTE: Ee know that message end is in the buffer, so +1 here is allowed to
//            // also get the end of message '\n' in the file.
//            if ((iRet = file_write(psClient->psDataFile, psClient->acRecvBuff,
//                                   (int32_t) (pcEnd - psClient->acRecvBuff + 1))) != 0) {
//                do_thread_exit_with_errno(__LINE__, iRet);
//            }
//
//            if ((iRet = file_send(psClient, psClient->psDataFile)) != 0) {
//                do_thread_exit_with_errno(__LINE__, iRet);
//            }
//        }
//    }
//
//    do_thread_exit_with_errno(__LINE__, iRet);
//}
//



//
///* Description:
// * Send complete file through socket to the client, threadsafe
// *
// * Return:
// * - errno on error
// * - RET_OK when succeeded
// */
//static int32_t file_send(sClient *psClient, sDataFile *psDataFile) {
//
//    int32_t iRet;
//
//    if ((psDataFile->pFile = fopen(psDataFile->pcFilePath, "r")) == NULL) {
//        iRet = errno;
//        goto exit_no_open;
//    }
//
//    /* Send complete file */
//    if (fseek(psDataFile->pFile, 0, SEEK_SET) != 0) {
//        iRet = errno;
//        goto exit;
//    }
//
//    while (!feof(psDataFile->pFile)) {
//        //NOTE: fread will return nmemb elements
//        //NOTE: fread does not distinguish between end-of-file and error,
//        int32_t iRead = fread(psClient->acSendBuff, 1, sizeof(psClient->acSendBuff), psDataFile->pFile);
//        if (ferror(psDataFile->pFile) != 0) {
//            iRet = errno;
//            goto exit;
//        }
//
//        if (send(psClient->iSockfd, psClient->acSendBuff, iRead, 0) < 0) {
//            iRet = errno;
//            goto exit;
//        }
//    }
//
//    iRet = RET_OK;
//
//    exit:
//    fclose(sGlobalDataFile.pFile);
//
//    exit_no_open:
//
//    return iRet;
//}
