#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include <sds.h>

#include <cew_http.h>
#include <cew_client.h>
#include <cew_logger.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* Parse the request header from the client, return the file requested to the caller
 *
 * https://linux.die.net/man/3/regcomp
 * */
static int32_t http_parse_request(tsClientStruct *psClient, sds *sFileName) {

    /* Regex parse */
    regex_t Regex;
    regmatch_t MatchGroups[2];
    regoff_t off, len;

    /* GET /a_page HTTP/1.1
     * 1 matching group
     * */
    if (regcomp(&Regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED | REG_ICASE) == 0) {

        if (regexec(&Regex, psClient->acRecvBuff, 2, MatchGroups, 0) == 0) {

            /* Calculate offset and length of the get request, then export it to sFileName */
            off = MatchGroups[1].rm_so;
            len = MatchGroups[1].rm_eo - MatchGroups[1].rm_so;

            *sFileName = sdscatlen(*sFileName, &psClient->acRecvBuff[off], len);

            /* Always lower case */
            sdstolower(*sFileName);

            /* Ditch URL encoding */
            //TODO

            log_debug("Requested file:%s", *sFileName);

            regfree(&Regex);

            return EXIT_SUCCESS;
        }
        regfree(&Regex);
    }
    return EXIT_FAILURE;
}

/* Return a complete 404 as a response */
static int32_t http_404(sds *sResponse) {

    log_debug("sending 404");

    *sResponse = sdscat(*sResponse, "HTTP/1.1 404 Not Found\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "\r\n"
                                    "404 Not Found");

    return EXIT_SUCCESS;
}

/* Make the fill path based on the provided filename.
 * Path will stay empty with a fail.
 *
 * When filename is empty it is assumed to be the main index.html
 *
 * */
static int32_t http_check_access_file(sds *sFileName, sds *sPath) {

    log_debug("Checking file:%s", *sFileName);

    char cSearchPath[] = "/opt/www";    //TODO

    if (strncmp(*sFileName, "", 1) == 0) {   // default
        *sFileName = sdscat(*sFileName, "index.html");  //TODO
    }

    *sPath = sdscatprintf(*sPath, "%s/%s", cSearchPath, *sFileName);
    log_debug("Made path: %s", *sPath);

    if (access(*sPath, F_OK) == -1) {  // file exists
        log_error("No access to file: %s with errno:%d", *sPath, errno);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Based on the filename, find the extension, and match a mime type
 * return sMime
 *
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
 * */
static int32_t http_determine_mime(sds *sFileName, sds *sMime) {

    log_debug("Determining mime type");

    uint8_t *cExt = NULL;

    uint8_t *cSearch = *sFileName;
    while ( (cSearch = strstr(cSearch, ".")) != NULL){
        cExt = cSearch;
        cSearch +=1;
    }

    if (cExt != NULL) {
        cExt += 1;

        if (strcmp(cExt, "html") == 0) {
            sdscat(*sMime, "text/html");
        } else if (strcmp(cExt, "txt") == 0) {
            sdscat(*sMime, "image/txt");
        } else if (strcmp(cExt, "css") == 0) {
            sdscat(*sMime, "text/css");
        } else if (strcmp(cExt, "jpg") == 0) {
            sdscat(*sMime, "image/jpeg");
        } else if (strcmp(cExt, "png") == 0) {
            sdscat(*sMime, "image/png");
        } else {
            log_error("Didn't fine mime type for file %s", *sFileName);
            return EXIT_FAILURE;
        }

        log_debug("Determined to be mime type: %s", *sMime);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

/* Parse the request and send back data */
int32_t http_handle_client_request(tsClientStruct *psClient) {

    psClient->acSendBuff = sdsempty();

    /* Check wat is requested */
    sds sFileName = sdsempty();
    if (http_parse_request(psClient, &sFileName) == EXIT_SUCCESS) {

        /* Check if we have the request */
        sds sPath = sdsempty();
        if (http_check_access_file(&sFileName, &sPath) == EXIT_SUCCESS) {

            /* What we need to return to the client */
            sds sMime = sdsempty();
            if (http_determine_mime(&sFileName, &sMime) == EXIT_SUCCESS) {

                log_info("Client requested: %s",sFileName);

                /* Add default header info */
                psClient->acSendBuff = sdscat(psClient->acSendBuff, "HTTP/1.1 200 OK\r\n");
                psClient->acSendBuff = sdscatprintf(psClient->acSendBuff, "Content-Type: %s\r\n", sMime);
                psClient->acSendBuff = sdscat(psClient->acSendBuff, "Connection: Closed\r\n");

                /* Open content */
                int fd;
                if ((fd = open(sPath, O_RDONLY)) != -1) {

                    /* Get content size */
                    struct stat file_stat;
                    fstat(fd, &file_stat);
                    psClient->acSendBuff = sdscatprintf(psClient->acSendBuff, "Content-Length: %ld\r\n",
                                                        file_stat.st_size);
                    /* Delimiter from header to body */
                    psClient->acSendBuff = sdscat(psClient->acSendBuff, "\r\n");

                    /* Read from disk and add content to send buffer */
                    // https://github.com/antirez/sds?tab=readme-ov-file#zero-copy-append-from-syscalls
                    int oldlen = sdslen(psClient->acSendBuff);
                    psClient->acSendBuff = sdsMakeRoomFor(psClient->acSendBuff, file_stat.st_size);
                    read(fd, psClient->acSendBuff + oldlen, file_stat.st_size);
                    sdsIncrLen(psClient->acSendBuff, file_stat.st_size);

                    log_debug("Responding with: \n%s", psClient->acSendBuff);
                }
            }
            sdsfree(sMime);

        } else { // 404
            http_404(&psClient->acSendBuff);
        }
        sdsfree(sPath);
    }

    sdsfree(sFileName);

    if (send(psClient->iSockfd, psClient->acSendBuff, sdslen(psClient->acSendBuff), 0) == -1) {
        log_debug("Sending error on client %d", psClient->iId);
    }

    sdsfree(psClient->acSendBuff);
    psClient->acSendBuff = NULL;

    return EXIT_SUCCESS;
}
