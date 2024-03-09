#include <stdio.h>
#include <stdint.h>

#include <sds.h>

#include <cew_http.h>

//static int32_t http_header_generator(uint8_t *buff, int32_t size)

int32_t http_basic_response(sds *response) {

    *response = sdscat(*response, "HTTP/1.1 200 OK\r\n");
    *response = sdscat(*response, "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n");
    *response = sdscat(*response, "Content-Length: 88\r\n");
    *response = sdscat(*response, "Server: Apache/2.2.14 (Win32)\r\n");
    *response = sdscat(*response, "Content-Type: text/html\r\n");
    *response = sdscat(*response, "Connection: Closed\r\n\r\n");
    *response = sdscat(*response, "<html>\r\n");
    *response = sdscat(*response, "<body>\r\n");
    *response = sdscat(*response, "<h1>Hello, World!</h1>\r\n");
    *response = sdscat(*response, "</body>\r\n");
    *response = sdscat(*response, "</html>\r\n");

}
