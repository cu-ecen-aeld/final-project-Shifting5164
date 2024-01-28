#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <ctype.h>

#include <settings.h>

#include <ini.h>
#include <sds.h>

sSettingsStruct sSettings = {0};

typedef enum eSettingsType{
    TYPE_NONE,
    TYPE_LONG,
    TYPE_STRING,
}eSettingsType;

typedef struct sOptionMapping {
    uint8_t *pSection;
    uint8_t *pKey;
    eSettingsType eType;
    void *pDst;
} sOptionMapping;

static sOptionMapping sKnownOptions[] = {
        {"general", "workers", TYPE_LONG,&sSettings.cWorkerThreads},
        {"general", "clients", TYPE_LONG,&sSettings.cMaxClientsPerThread},
        {NULL, NULL, TYPE_NONE,NULL}
};

static int32_t parse_option(uint8_t *cpcSection, uint8_t *cpcKey, const uint8_t *cpcValue) {
    sOptionMapping *pMap = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(sOptionMapping);

    for (int i = 0; i < iSize-1; i++) {

        pMap = &sKnownOptions[i];

        assert(pMap != NULL);
        assert(pMap->pSection != NULL);

        printf("pMap->pSection: %s:%s\n", cpcSection, pMap->pSection);
        printf("pMap->pKey: %s:%s\n", cpcKey, pMap->pKey);

        if ((strncmp(pMap->pSection, cpcSection, strlen(pMap->pSection)) == 0) &&
            (strncmp(pMap->pKey, cpcKey, strlen(pMap->pKey)) == 0)) {

            switch (pMap->eType) {
                case TYPE_LONG:
                    long val = strtol(cpcValue, NULL, 10);
                    memcpy(pMap->pDst,&val, sizeof(long));

                    printf("*pMap->pDst: %ld\n", (long *)pMap->pDst);
                    printf("&pMap->pDst: %ld\n", &pMap->pDst);

                    printf("cMaxClientsPerThread: %ld\n", &sSettings.cMaxClientsPerThread);
                    printf("cWorkerThreads: %ld\n", &sSettings.cWorkerThreads);
                    printf("cLogLevel: %ld\n", &sSettings.cLogLevel);

                    break;
                case TYPE_STRING:
                    //TODO
                    break;
                default:
                    return -1;
            }
        }
    }

    return 0;
}

sSettingsStruct *settings_init(void) {
    return &sSettings;
}

int32_t settings_destroy(void) {
    return 0; //TODO
}

int32_t settings_load(const uint8_t *cpSettingsFile) {
    struct INI *ini = NULL;

    ini = ini_open(cpSettingsFile);
    if (!ini) {
        return EXIT_FAILURE;
    }

    printf("INI file opened.\n");

    while (1) {
        const char *cpcBuf;
        char *pcSection;
        size_t SectionLen;

        int iRet = ini_next_section(ini, &cpcBuf, &SectionLen);
        if (!iRet) {
            printf("End of file.\n");
            break;
        }

        if (iRet < 0) {
            printf("ERROR: code %i\n", iRet);
            goto error;
        }

        pcSection = alloca(SectionLen + 1);
        pcSection[SectionLen] = '\0';
        memcpy(pcSection, cpcBuf, SectionLen);
        printf("Opening section: \'%s\'\n", pcSection);

        while (1) {
            const char *buf2;
            char *pcKey, *pcValue;
            size_t KeyLen, ValueLen;

            iRet = ini_read_pair(ini, &cpcBuf, &KeyLen, &buf2, &ValueLen);
            if (!iRet) {
                printf("No more data.\n");
                break;
            }

            if (iRet < 0) {
                printf("ERROR: code %i\n", iRet);
                goto error;
            }

            pcKey = alloca(KeyLen + 1);
            pcKey[KeyLen] = '\0';
            memcpy(pcKey, cpcBuf, KeyLen);
            pcValue = alloca(ValueLen + 1);
            pcValue[ValueLen] = '\0';
            memcpy(pcValue, buf2, ValueLen);
            printf("Reading pcKey: \'%s\' pcValue: \'%s\'\n", pcKey, pcValue);

            for(int i = 0; pcSection[i]; i++){
                pcSection[i] = tolower(pcSection[i]);
            }

            for(int i = 0; pcKey[i]; i++){
                pcKey[i] = tolower(pcKey[i]);
            }

            parse_option(pcSection, pcKey, pcValue);

        }
    }

    ini_close(ini);

    printf("Loaded cMaxClientsPerThread: %ld\n", sSettings.cMaxClientsPerThread);
    printf("Loaded cWorkerThreads: %ld\n", sSettings.cWorkerThreads);

    return EXIT_SUCCESS;


    error:
    ini_close(ini);
    return EXIT_FAILURE;
}
