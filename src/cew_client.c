#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ev.h>
#include <sds.h>

#include <cew_client.h>
#include <cew_logger.h>
#include <cew_worker.h>
#include <cew_http.h>

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

    /* First shutdown the socket, this will send and end to the client. Reading and writing will
     * be disallowed. After disallowing, make sure we read all the data that presists on the socket
     * before doing a close, to make sure it's a clean close.
     *
     * Without a shutdown, the client still things its connected, and waits for data.
     * */
    if (psClientEv->sClient.iSockfd) {
        shutdown(psClientEv->sClient.iSockfd, SHUT_RDWR);
        int8_t trash[100];
        while (read(psClientEv->sClient.iSockfd,&trash, sizeof(trash)));
        close(psClientEv->sClient.iSockfd);
        psClientEv->sClient.iSockfd = 0;
    }

    if (psClientEv->sClient.acSendBuff) {
        sdsfree(psClientEv->sClient.acSendBuff);
        psClientEv->sClient.acSendBuff = NULL;
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

    int32_t iReceived = recv(psClient->iSockfd, psClient->acRecvBuff, RECV_BUFF_SIZE, 0);
    if (iReceived < 0) {
        /* Error */
        log_error("Receive error on client %d.", psClient->iId);
    } else if (iReceived == 0) {
        /* This is the only way a client can disconnect */
        log_debug("Disconnecting client %d", psClient->iId);
        client_deregister(psClientEv);
    } else {
        /* Got data from client, do stuff */
        log_debug("Received data %s", psClient->acRecvBuff);

        ev_timer_again(psWorkerEVLoop, &psClientEv->timer);

        http_handle_client_request(psClient);

        client_deregister(psClientEv);

    }
}

/* Dynamically register a client and callbacks for the client */
int32_t client_register_ev(int32_t iSockfd) {

    tsClientEv *psClientEv = malloc(sizeof(struct sClientEv));
    memset(psClientEv, 0, sizeof(struct sClientEv));

    psClientEv->sClient.iSockfd = iSockfd;
    psClientEv->sClient.iId = (int32_t) random();

    // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
    uint32_t uiLen = sizeof(psClientEv->sClient.sTheirAddr);

    if (getpeername(psClientEv->sClient.iSockfd, (struct sockaddr *)&psClientEv->sClient.sTheirAddr, &uiLen) == -1) {
        return EXIT_FAILURE;
    }

    if (inet_ntop(AF_INET, &(psClientEv->sClient.sTheirAddr.sin_addr), psClientEv->sClient.cIP, sizeof(psClientEv->sClient.cIP)) == NULL) {
        return EXIT_FAILURE;
    }

    log_info("Added callback for client %d from %s ", psClientEv->sClient.iId, psClientEv->sClient.cIP);

    /* Client receive timeout timer */
    ev_timer_init (&psClientEv->timer, client_callback_timeout, CLIENT_TIMEOUT, CLIENT_TIMEOUT);
    ev_timer_start(psWorkerEVLoop, &psClientEv->timer);

    /* Client serving callback */
    ev_io_init(&psClientEv->io, client_callback_serve, psClientEv->sClient.iSockfd, EV_READ);
    ev_io_start(psWorkerEVLoop, &psClientEv->io);

    return EXIT_SUCCESS;
}

