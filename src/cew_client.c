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

/* Remove a client from ev because a timeout */
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

/* serve the client with data */
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

/* Dynamically register a client and callbacks for the client */
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

