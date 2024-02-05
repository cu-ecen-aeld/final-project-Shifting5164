#ifndef __SETTINGS_H
#define __SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include <sds.h>

typedef struct sSettings {
    long lMaxClientsPerThread;
    long lWorkerThreads;
    long lLogLevel;
    sds pcLogfile;
} sSettingsStruct;

sSettingsStruct *settings_init(void);
int32_t settings_destroy(void);

int32_t settings_load(const uint8_t *);


#endif
