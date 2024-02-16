#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/resource.h>

#include <ev.h>

#include <settings.h>
#include <logger.h>

/*
https://github.com/cu-ecen-aeld/final-project-Shifting5164
*/

//#define SOCKET_FAIL -1
//#define RET_OK 0
//
///* socket send and recv buffers, per client */
//#define RECV_BUFF_SIZE 1024     /* bytes */
//#define SEND_BUFF_SIZE 1024     /* bytes */
//
///* Timpestamp interval */
//#define TIMESTAMP_INTERVAL 10 /* seconds */
//#define HOUSECLEANING_INTERVAL 100 /* ms */
//
///* Global datafile or kernel buffer*/
//#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
//
//
//typedef struct DataFile {
//    char *pcFilePath;           /* path */
//    FILE *pFile;               /* filehandle */
//    pthread_mutex_t pMutex;     /* thread safe datafile access */
//} sDataFile;
//
//sDataFile sGlobalDataFile = {
//        .pcFilePath = DATA_FILE_PATH,
//        .pFile = NULL,
//        .pMutex = PTHREAD_MUTEX_INITIALIZER
//};
//
///* Connected client info */
//typedef struct sClient {
//    int32_t iSockfd;         /* socket clients connected on */
//    bool bIsDone;             /* client disconnected  ? */
//    sDataFile *psDataFile;  /* global data file to use*/
//    struct sockaddr_storage sTheirAddr;
//    socklen_t tAddrSize;
//    char acSendBuff[SEND_BUFF_SIZE];    /* data sending */
//    char acRecvBuff[RECV_BUFF_SIZE];    /* data reception */
//    int32_t iReceived;
//} sClient;
//
///* List struct for client thread tracking */
//typedef struct sClientThreadEntry {
//    pthread_t sThread;      /* pthread info */
//    sClient sClient;        /* connected client information */
//    long lID;               /* random unique id of the thread*/
//    LIST_ENTRY(sClientThreadEntry) sClientThreadEntry;
//} sClientThreadEntry;
//
//#define BACKLOG 10
//#define PORT "9000"
//
//int32_t iSfd = 0;      /* connect socket */
//bool bTerminateProg = false; /* terminating program gracefully */
//pthread_t Cleanup;      /* cleanup thread */
//
//
///* Thread list for clients connections
// * List actions thread safe with 'ListMutex'
// * */
//LIST_HEAD(listhead, sClientThreadEntry);
//struct listhead sClientThreadListHead;
//pthread_mutex_t ListMutex = PTHREAD_MUTEX_INITIALIZER;
//
///* Loop the thread list, thread safe.
// * Cleanup thread sClientThreadEntry that are not serving clients anymore
// * optionally return piStillActive
// */
//static int32_t cleanup_client_list(int32_t *piStillActive) {
//
//    int32_t iCount = 0;
//    sClientThreadEntry *curr = NULL, *next = NULL;
//
//    /* Note: when looping the list, and something needs to be removed, then start the
//     * list loop again to prevent pointer errors and memory leaks */
//    int iSomethingRemoved;
//    do {
//        iSomethingRemoved = 0;
//
//        if (pthread_mutex_lock(&ListMutex) != 0) {
//            return errno;
//        }
//
//        LIST_FOREACH(curr, &sClientThreadListHead, sClientThreadEntry) {
//            if (curr->sClient.bIsDone == true) {
//                pthread_join(curr->sThread, NULL);
//                printf("Done with client thread: %ld\n", curr->lID);
//                LIST_REMOVE(curr, sClientThreadEntry);
//                free(curr);
//                iSomethingRemoved++;
//                continue; /* loop list again */
//            } else {
//                iCount++;   /* count still active clients, to be returned to the caller */
//            }
//        }
//
//        if (pthread_mutex_unlock(&ListMutex) != 0) {
//            return errno;
//        }
//
//    } while (iSomethingRemoved);
//
//    /* Optionally return amount of still connected clients */
//    if (piStillActive != NULL) {
//        *piStillActive = iCount;
//    }
//
//    return RET_OK;
//}
//
///* completing any open connection operations,
// * closing any open sockets, and deleting the file /var/tmp/aesdsocketdata*/
//static void exit_cleanup(void) {
//
//    /* End support threads */
//    pthread_cancel(Cleanup);
//    pthread_join(Cleanup, NULL);
//
//    /* Remove datafile */
//    if (sGlobalDataFile.pFile != NULL) {
//        unlink(sGlobalDataFile.pcFilePath);
//    }
//
//    /* Close socket */
//    if (iSfd > 0) {
//        close(iSfd);
//    }
//
//    /* No more syslog needed */
//    closelog();
//}
//
///* Signal actions */
//void sig_handler(const int ciSigno) {
//
//    if (ciSigno != SIGINT && ciSigno != SIGTERM) {
//        return;
//    }
//
//    syslog(LOG_INFO, "Got signal: %d", ciSigno);
//
//    bTerminateProg = true;
//}
//
//static void do_exit(const int32_t ciExitval) {
//    exit_cleanup();
//    printf("Goodbye!\n");
//    exit(ciExitval);
//}
//
//static void do_thread_exit_with_errno(const int32_t ciLine, const int32_t ciErrno) {
//    fprintf(stderr, "Exit with %d: %s. Line %d.\n", ciErrno, strerror(ciErrno), ciLine);
//    pthread_exit((void *) ciErrno);
//}
//
//static void do_exit_with_errno(const int32_t ciLine, const int32_t ciErrno) {
//    fprintf(stderr, "Exit with %d: %s. Line %d.\n", ciErrno, strerror(ciErrno), ciLine);
//    do_exit(ciErrno);
//}
//
///* Description:
// * Setup signals to catch
// *
// * Return:
// * - errno on error
// * - RET_OK when succeeded
// */
//static int32_t setup_signals(void) {
//
//    /* SIGINT or SIGTERM terminates the program with cleanup */
////    struct sigaction sSigAction = {0};
//
//    struct sigaction sSigAction;
//
//    sigemptyset(&sSigAction.sa_mask);
//
//    sSigAction.sa_flags = 0;
//    sSigAction.sa_handler = sig_handler;
//
//    if (sigaction(SIGINT, &sSigAction, NULL) != 0) {
//        return errno;
//    }
//
//    if (sigaction(SIGTERM, &sSigAction, NULL) != 0) {
//        return errno;
//    }
//
//    return RET_OK;
//}
//
///* Description:
// * Setup socket handling
// * https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#system-calls-or-bust
// *
// * Return:
// * - errno on error
// * - RET_OK when succeeded
// */
//static int32_t setup_socket(void) {
//
//    struct addrinfo sHints = {0};
//    struct addrinfo *psServinfo = NULL;
//
//    memset(&sHints, 0, sizeof(sHints)); // make sure the struct is empty
//    sHints.ai_family = AF_INET;
//    sHints.ai_socktype = SOCK_STREAM; // TCP stream sockets
//    sHints.ai_flags = AI_PASSIVE;     // bind to all interfaces
//
//    if ((getaddrinfo(NULL, PORT, &sHints, &psServinfo)) != 0) {
//        return errno;
//    }
//
//    if ((iSfd = socket(psServinfo->ai_family, psServinfo->ai_socktype, psServinfo->ai_protocol)) < 0) {
//        return errno;
//    }
//
//    // lose the pesky "Address already in use" error message
//    int32_t iYes = 1;
//    if (setsockopt(iSfd, SOL_SOCKET, SO_REUSEADDR, &iYes, sizeof iYes) == -1) {
//        return errno;
//    }
//
//    if (bind(iSfd, psServinfo->ai_addr, psServinfo->ai_addrlen) < 0) {
//        return errno;
//    }
//
//    /* psServinfo not needed anymore */
//    freeaddrinfo(psServinfo);
//
//    if (listen(iSfd, BACKLOG) < 0) {
//        return errno;
//    }
//
//    return RET_OK;
//}
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
//
//
///* Description:
// * Write cvpBuff with ciSize to datafile, threadsafe
// *
// * Return:
// * - errno on error
// * - RET_OK when succeeded
// */
//static int32_t file_write(sDataFile *psDataFile, const void *cvpBuff, const int32_t ciSize) {
//
//    int32_t iRet = RET_OK;
//
//    if (pthread_mutex_lock(&psDataFile->pMutex) != 0) {
//        return errno;
//    }
//
//    if ((psDataFile->pFile = fopen(psDataFile->pcFilePath, "a+")) == NULL) {
//        iRet = errno;
//        goto exit_no_open;
//    }
//
//    /* Append received data */
//    fwrite(cvpBuff, ciSize, 1, psDataFile->pFile);
//    if (ferror(psDataFile->pFile) != 0) {
//        iRet = errno;
//    }
//
//    fclose(sGlobalDataFile.pFile);
//
//    exit_no_open:
//    pthread_mutex_unlock(&psDataFile->pMutex);
//
//    return iRet;
//}
//
//static int32_t daemonize(void) {
//
//    /* Clear file creation mask */
//    umask(0);
//
//    /* Get fd limts for later */
//    struct rlimit sRlim;
//    if (getrlimit(RLIMIT_NOFILE, &sRlim) < 0) {
//        fprintf(stderr, "Can't get file limit. Line %d.\n", __LINE__);
//        do_exit(1);
//    }
//
//    /* Session leader */
//    pid_t pid;
//    if ((pid = fork()) < 0) {
//        return errno;
//    } else if (pid != 0) {
//
//        /* Deamonizing is happening at the beginning of the program, and releases the
//         * initial process directly after, this doesn't mean that the program is ready to accept() anything. Thus my
//         * startup deamonizing is too fast for the unittest. So exit the main process when the program can accept()
//         * so the unitest doesn't fail :-) guestimating at 1 second */
//        sleep(1);
//
//        /* Exit parent */
//        exit(EXIT_SUCCESS);
//    }
//    setsid();
//
//    /* Disallow future opens won't allocate controlling TTY's */
//    struct sigaction sa;
//    sa.sa_handler = SIG_IGN;
//    sigemptyset(&sa.sa_mask);
//    sa.sa_flags = 0;
//    if (sigaction(SIGHUP, &sa, NULL) < 0) {
//        return errno;
//    }
//
//    /* real fork */
//    if ((pid = fork()) < 0) {
//        return errno;
//    } else if (pid != 0) {
//        /* Exit parent */
//        exit(EXIT_SUCCESS);
//    }
//
//    if (chdir("/") < 0) {
//        return errno;
//    };
//
//    /* Close all fd's */
//    if (sRlim.rlim_max == RLIM_INFINITY) {
//        sRlim.rlim_max = 1024;
//    }
//
//    int i;
//    for (i = 0; i < sRlim.rlim_max; i++) {
//        close(i);
//    }
//
//    /* Attach fd 0/1/2 to /dev/null */
//    int32_t fd0, fd1, fd2;
//    fd0 = open("/dev/null", O_RDWR);
//    fd1 = dup(0);
//    fd2 = dup(0);
//
//    /* init syslog */
//    openlog(NULL, 0, LOG_USER);
//
//    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
//        fprintf(stderr, "Error setting up file descriptors. Line %d.\n", __LINE__);
//        do_exit(1);
//    }
//
//    return RET_OK;
//}
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
///* handle finished client */
//static void *housekeeping(void *arg) {
//
//    while (1) {
//        /* Loop list, see if a thread is done. When it is then remove from the list */
//        cleanup_client_list(NULL);
//
//        usleep(HOUSECLEANING_INTERVAL * 1000);
//    }
//}

//int32_t main(int32_t argc, char **argv) {
int32_t main(void) {

//    bool bDeamonize = false;
    int32_t iRet = 0;
//
//    if ((argc > 1) && strcmp(argv[0], "-d")) {
//        bDeamonize = true;
//    }

    if ( (iRet = logger_init("/var/log/testlog", eDEBUG)) != 0 ){
        return iRet;
    }

    log_error("----- STARTING -------");
    log_warning("----- STARTING -------");
    log_info("----- STARTING -------");
    log_debug("----- STARTING -------");


    if ((iRet = log_error("logmsg %d", 42))!= 0 ){
        return iRet;
    }

    settings_init();
    settings_load("/work/test/ini/valid_dual_settings.ini");

    logger_destroy();
    exit(0);
//
//    /* Going to run as service or not > */
//    if (bDeamonize) {
//        printf("Demonizing, listening on port %s\n", PORT);
//        if ((iRet = daemonize()) != 0) {
//            do_exit_with_errno(__LINE__, iRet);
//        }
//    }
//
//    if ((iRet = setup_signals()) != RET_OK) {
//        do_exit_with_errno(__LINE__, iRet);
//    }
//
//    /* Init the client thread list */
//    LIST_INIT (&sClientThreadListHead);
//
//    /* spinup housekeeping thread to handle finished client connections */
//    if (pthread_create(&Cleanup, NULL, housekeeping, NULL) != 0) {
//        do_exit_with_errno(__LINE__, errno);
//    }
//
//    /* Opens a stream socket, failing and returning -1 if any of the socket connection steps fail. */
//    if ((iRet = setup_socket()) != RET_OK) {
//        fprintf(stderr, "Exit with %d: %s. Line %d.\n", iRet, strerror(iRet), __LINE__);
//        do_exit(SOCKET_FAIL);
//    }
//
//    if (!bDeamonize) {
//        printf("Waiting for connections...\n");
//    }
//
//    /* Keep receiving clients */
//    while (1) {
//
//        /* Found a exit signal */
//        if (bTerminateProg == true) {
//            break;
//        }
//
//        /* tmp allocation of client data, to be copied to thead info struct later */
//        int32_t iSockfd;
//        struct sockaddr_storage sTheirAddr;
//        socklen_t tAddrSize = sizeof(sTheirAddr);
//
//        /* Accept clients, and fill client information struct, BLOCKING  */
//        if ((iSockfd = accept(iSfd, (struct sockaddr *) &sTheirAddr, &tAddrSize)) < 0) {
//
//            /* crtl +c */
//            if (errno != EINTR) {
//                do_exit_with_errno(__LINE__, errno);
//            } else {
//                break;
//            }
//        }
//
//        /* prepare thread  item */
//        sClientThreadEntry *psClientThreadEntry = NULL;
//        if ((psClientThreadEntry = calloc(sizeof(sClientThreadEntry), 1)) == NULL) {
//            do_exit_with_errno(__LINE__, errno);
//        }
//
//        /* Copy connect data from accept */
//        psClientThreadEntry->sClient.iSockfd = iSockfd;
//        psClientThreadEntry->sClient.tAddrSize = tAddrSize;
//        memcpy(&psClientThreadEntry->sClient.sTheirAddr, &sTheirAddr, sizeof(sTheirAddr));
//
//        /* Link global data file */
//        psClientThreadEntry->sClient.psDataFile = &sGlobalDataFile;
//
//        psClientThreadEntry->sClient.bIsDone = false;
//
//        /* Add random ID for tracking */
//        psClientThreadEntry->lID = random();
//
//        printf("Spinning up client thread: %ld\n", psClientThreadEntry->lID);
//
//        /* Insert client thread tracking on list sClientThreadListHead */
//        if (pthread_mutex_lock(&ListMutex) != 0) {
//            do_exit_with_errno(__LINE__, errno);
//        }
//
//        LIST_INSERT_HEAD(&sClientThreadListHead, psClientThreadEntry, sClientThreadEntry);
//        if (pthread_mutex_unlock(&ListMutex) != 0) {
//            do_exit_with_errno(__LINE__, errno);
//        }
//
//        /* Spawn new thread and serve the client */
//        if (pthread_create(&psClientThreadEntry->sThread, NULL, client_serve, &psClientThreadEntry->sClient) < 0) {
//            do_exit_with_errno(__LINE__, errno);
//        }
//    }
//
//    do_exit(EXIT_SUCCESS);
}
