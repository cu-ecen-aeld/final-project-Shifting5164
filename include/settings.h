#ifndef CEWSERVER_SETTINGS_H
#define CEWSERVER_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include <sds.h>

/* All settings for the project */
typedef struct sSSettings {
    long lMaxClientsPerThread;
    long lWorkerThreads;
    long lLogLevel;
    sds pcLogfile;
} tsSSettings;

#define SET_EXIT_SUCCESS EXIT_SUCCESS
#define SET_EXIT_FAILURE EXIT_FAILURE   // + errno usually
#define SET_NOTYPE -1
#define SET_NOMAP -2
#define SET_NOFILE -3


tsSSettings *settings_init(void);

int32_t settings_destroy(void);

int32_t settings_load(const char*);


#endif
