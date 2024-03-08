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
void socket_accept_client(struct ev_loop *loop, ev_io *w_, int revents) {

    struct sockaddr_storage sTheirAddr;
    socklen_t tAddrSize;
    int32_t iSockfd;

    log_debug("Master. Got Callback from ev about a client.");

    /* Accept clients, and fill client information struct */
    iSockfd = accept(iFd, (struct sockaddr *) &sTheirAddr, &tAddrSize);
    if (iSockfd == -1) {
        log_error("Master. Error with accepting client.");
        return;
    }

    log_debug("Sending fd:%d to the worker.", iSockfd);

    /* Route new client to a worker */
    if (worker_route_client(&iSockfd) != WORKER_EXIT_SUCCESS) {
        log_warning("Master. Couldn't route client to worker!");
    }
}

/* Description:
 * Setup socket handling
 * https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#system-calls-or-bust
 *
 * Return:
 * - errno on error
 * - RET_OK when succeeded
 */
int32_t socket_setup(uint16_t iPort, int32_t *iRetFd) {

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

    // https://www.man7.org/linux/man-pages/man7/socket.7.html
    // Permits multiple AF_INET or AF_INET6 sockets to be bound to an identical socket address.
    if (setsockopt(iFd, SOL_SOCKET, SO_REUSEPORT, &iYes, sizeof iYes) == -1) {
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

    *iRetFd = iFd;

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

