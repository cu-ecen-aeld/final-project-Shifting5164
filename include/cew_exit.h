#ifndef CEWSERVER_CEW_EXIT_H
#define CEWSERVER_CEW_EXIT_H

#include <stdbool.h>
#include <stdint.h>

extern bool bTerminateProg;

void do_exit(int32_t);

void do_exit_with_errno(int32_t);

#endif //CEWSERVER_CEW_EXIT_H
