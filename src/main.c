#include <stdio.h>
#include "../external/libini/ini.h"

int main() {

    struct INI *ini;

    ini = ini_open("test.ini");
    if (!ini) {
        printf("No file.\n");
        return EXIT_FAILURE;
    }
    printf("INI file opened.\n");

    printf("Hello, World!\n");
    return 0;
}
