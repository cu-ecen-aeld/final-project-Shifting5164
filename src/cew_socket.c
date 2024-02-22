#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <cew_socket.h>
#include <cew_logger.h>
#include <cew_client.h>

/* connect socket */
static int32_t iFd = 0;

// blocking accept
//todo libev ?
int32_t socket_accept_client(tsClientStruct *psNewClient) {

    if (!psNewClient) {
        return -1;
    }

    log_debug("Waiting for accept");

    /* Accept clients, and fill client information struct, BLOCKING  */
    if ((psNewClient->iSockfd = accept(iFd, (struct sockaddr *) &psNewClient->sTheirAddr, &psNewClient->tAddrSize)) ==
        -1) {

        /* crtl +c */
        if (errno != EINTR) {
            exit(0);        //TODO
        } else {
            return -1;
        }
    }

    psNewClient->iId = (int32_t) random();
    log_debug("Got client %d from accept.", psNewClient->iId);

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

    // lose the pesky "Address already in use" error message
    int32_t iYes = 1;
    if (setsockopt(iFd, SOL_SOCKET, SO_REUSEADDR, &iYes, sizeof iYes) == -1) {
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

