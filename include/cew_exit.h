#ifndef CEWSERVER_CEW_EXIT_H
#define CEWSERVER_CEW_EXIT_H

#include <stdbool.h>

extern bool bTerminateProg;

void do_exit(const int32_t);

void do_exit_with_errno(const int32_t);

#endif //CEWSERVER_CEW_EXIT_H
