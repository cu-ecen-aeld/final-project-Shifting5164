#ifndef CEWSERVER_CEW_SETTINGS_H
#define CEWSERVER_CEW_SETTINGS_H

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
#define SET_NOTYPE (-1)
#define SET_NOMAP (-2)
#define SET_NOFILE (-3)

int32_t settings_init(void);

int32_t settings_destroy(void);

/* Load settings from settings file */
int32_t settings_load(const char *);

/* Get a copy of the settings */
tsSSettings settings_get(void);

/* Set new settings */
int32_t settings_set(tsSSettings);

#endif //CEWSERVER_CEW_SETTINGS_H
