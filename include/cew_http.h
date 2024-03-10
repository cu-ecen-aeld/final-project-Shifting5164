#ifndef CEWSERVER_CEW_HTTP_H
#define CEWSERVER_CEW_HTTP_H

#include <stdint.h>
#include <sds.h>
#include <cew_client.h>

int32_t http_handle_client_request(tsClientStruct *);

#endif //CEWSERVER_CEW_HTTP_H
