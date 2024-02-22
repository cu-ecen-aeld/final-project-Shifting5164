#include <malloc.h>
#include <string.h>

#include <cew_client.h>

// will malloc psNewClient
//todo return codes
int32_t client_init(tsClientStruct **ppsNewClient) {
    *ppsNewClient = malloc(sizeof(struct sClientStruct));
    memset(*ppsNewClient, 0, sizeof(struct sClientStruct));
    return 0;
}

void client_destroy(tsClientStruct *psClient) {
    if (psClient == NULL) {
        return;
    }

    close(psClient->iSockfd);   //todo check if open
    free(psClient);
}



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
