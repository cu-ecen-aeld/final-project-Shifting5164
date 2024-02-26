#include <stdint.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ev.h>

#include <cew_worker.h>
#include <cew_socket.h>
#include <cew_logger.h>
#include <cew_client.h>

/* connect socket */
static int32_t iFd = 0;

/* Callback function for ev polling. Accepts the clients, makes a new fd, and routes the
 * client to a worker. */
static void socket_accept_client(EV_P_ ev_io *w, int revents) {

    tsClientStruct *psNewClient = NULL;
    client_init(&psNewClient);

    log_debug("Got Callback from ev about a client.");

    /* Accept clients, and fill client information struct */
    psNewClient->iSockfd = accept(iFd, (struct sockaddr *) &psNewClient->sTheirAddr, &psNewClient->tAddrSize);
    if (psNewClient->iSockfd == -1) {
        free(psNewClient);
        log_error("Error with accepting client.");
        return;
    }

    psNewClient->iId = (int32_t) random(); // TODO move to client_init

    log_debug("Got client %d from accept. Going to route it to a worker.", psNewClient->iId);

    /* Route new client to a worker */
    worker_route_client(psNewClient);
}

/* Setup the polling and callback for new clients. New clients will be received on the
 * non-blocking, already open iFd.
 *
 * This function should never return.
 *
 * http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#code_ev_io_code_is_this_file_descrip
 */
int32_t socket_poll(void) {
    struct ev_loop *psLoop = ev_default_loop(0);
    ev_io ClientWatcher;

    /* Setup the callback for client notification */
    ev_io_init(&ClientWatcher, socket_accept_client, iFd, EV_READ);

    ev_io_start(psLoop, &ClientWatcher);
    ev_run(psLoop, 0);

    return 0;
}

/* Description:
 * Setup socket handling
 * https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#system-calls-or-bust
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t socket_setup(uint16_t iPort) {

    struct addrinfo sHints = {0};
    struct addrinfo *psServinfo = NULL;

    memset(&sHints, 0, sizeof(sHints)); // make sure the struct is empty
    sHints.ai_family = AF_INET;
    sHints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    sHints.ai_flags = AI_PASSIVE;     // bind to all interfaces

    char cPort[6] = {0}; //65535\0
    snprintf(cPort, sizeof(cPort), "%d", iPort);

    if ((getaddrinfo(NULL, cPort, &sHints, &psServinfo)) != 0) {
        return errno;
    }

    if ((iFd = socket(psServinfo->ai_family, psServinfo->ai_socktype, psServinfo->ai_protocol)) < 0) {
        return errno;
    }

    /* lose the pesky "Address already in use" error message */
    int32_t iYes = 1;
    if (setsockopt(iFd, SOL_SOCKET, SO_REUSEADDR, &iYes, sizeof iYes) == -1) {
        return errno;
    }

    /* non-blocking socket settings */
    if (fcntl(iFd, F_SETFL, O_NONBLOCK) == -1) {
        return errno;
    }

    if (bind(iFd, psServinfo->ai_addr, psServinfo->ai_addrlen) < 0) {
        return errno;
    }

    /* psServinfo not needed anymore */
    freeaddrinfo(psServinfo);

    if (listen(iFd, BACKLOG) < 0) {
        return errno;
    }

    /* non-blocking socket settings */
    if (fcntl(iFd, F_SETFL, O_NONBLOCK) == -1) {
        return errno;
    }

    log_info("Socket listing on port %d", iPort);

    return SOCK_EXIT_SUCCESS;
}


int32_t socket_close(void) {

    /* Close socket */
    if (iFd > 0) {
        close(iFd);
        iFd = 0;
    }

    log_info("Stopped socket.");

    return 0;

}

