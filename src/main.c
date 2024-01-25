#include <stdio.h>
#include "../external/libini/ini.h"
#include "../external/libev/ev.h"

static void my_cb (struct ev_loop *loop, ev_io *w, int revents)
{
    ev_break (loop, EVBREAK_ALL);
}

int main() {

    struct INI *ini;

    ini = ini_open("test.ini");
    if (!ini) {
        printf("No file.\n");
    }
    printf("INI file opened.\n");

    struct ev_loop *loop = ev_default_loop (0);
    ev_io stdin_watcher;
    ev_init (&stdin_watcher, my_cb);

    return 0;
}
