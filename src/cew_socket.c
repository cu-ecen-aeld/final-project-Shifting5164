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

/* connect socket */
static int32_t iFd = 0;


int32_t socket_receive_client(void) {

    /* Found a exit signal */
//    if (bTerminateProg == true) {
//        break;
//    }

    /* tmp allocation of client data, to be copied to thead info struct later */
    int32_t iSockfd;
    struct sockaddr_storage sTheirAddr;
    socklen_t tAddrSize = sizeof(sTheirAddr);

    /* Accept clients, and fill client information struct, BLOCKING  */
    if ((iSockfd = accept(iFd, (struct sockaddr *) &sTheirAddr, &tAddrSize)) < 0) {

        /* crtl +c */
        if (errno != EINTR) {
//            do_exit_with_errno(__LINE__, errno);
            return -1;
        } else {
            return -1;
        }
    }

    char testbuff[] = "hello\n";
    send(iSockfd, testbuff, sizeof(testbuff), 0);

    return 0;

//
//    /* prepare thread  item */
//    sWorkerThreadEntry *psClientThreadEntry = NULL;
//    if ((psClientThreadEntry = calloc(sizeof(sWorkerThreadEntry), 1)) == NULL) {
//        do_exit_with_errno(__LINE__, errno);
//    }
//
//    /* Copy connect data from accept */
//    psClientThreadEntry->sClient.iSockfd = iSockfd;
//    psClientThreadEntry->sClient.tAddrSize = tAddrSize;
//    memcpy(&psClientThreadEntry->sClient.sTheirAddr, &sTheirAddr, sizeof(sTheirAddr));
//
//    psClientThreadEntry->sClient.bIsDone = false;
//
//    /* Add random ID for tracking */
//    psClientThreadEntry->lID = random();

//    printf("Spinning up client thread: %ld\n", psClientThreadEntry->lID);

//    /* Spawn new thread and serve the client */
//    if (pthread_create(&psClientThreadEntry->sThread, NULL, client_serve, &psClientThreadEntry->sClient) < 0) {
//        do_exit_with_errno(__LINE__, errno);
//    }

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

