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

typedef enum eSettingsType {
    TYPE_NONE,
    TYPE_LONG,
    TYPE_STRING,
} eSettingsType;

typedef struct sOptionMapping {
    uint8_t *pSection;
    uint8_t *pKey;
    eSettingsType eType;
    void *pvDst;
} tsOptionMapping;

static tsOptionMapping sKnownOptions[] = {
        {"general", "workers", TYPE_LONG,   &sSettings.lWorkerThreads},
        {"general", "clients", TYPE_LONG,   &sSettings.lMaxClientsPerThread},
        {"logging", "logfile", TYPE_STRING, &sSettings.pcLogfile},
        {"logging", "level",   TYPE_LONG,   &sSettings.lLogLevel},
        {NULL, NULL,           TYPE_NONE, NULL}
};

void show_settings(void) {
    tsOptionMapping *psSetting = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(tsOptionMapping);

    for (int i = 0; i < iSize - 1; i++) {

        psSetting = &sKnownOptions[i];

        assert(psSetting != NULL);
        assert(psSetting->pSection != NULL);

        switch (psSetting->eType) {
            case TYPE_LONG:
                printf("Setting %s:%s = %ld\n", psSetting->pSection, psSetting->pKey, *(long *) (psSetting->pvDst));
                break;

            case TYPE_STRING:
                printf("Setting %s:%s = %s\n", psSetting->pSection, psSetting->pKey, (char *) (psSetting->pvDst));
                break;

            default:
                break;
        }
    }
}

static int32_t parse_option(const uint8_t *cpcSection, const uint8_t *cpcKey, const uint8_t *cpcValue) {
    tsOptionMapping *psSetting = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(tsOptionMapping);

    for (int i = 0; i < iSize - 1; i++) {

        psSetting = &sKnownOptions[i];

        assert(psSetting != NULL);
        assert(psSetting->pSection != NULL);

        if ((strncmp(psSetting->pSection, cpcSection, strlen(psSetting->pSection)) == 0) &&
            (strncmp(psSetting->pKey, cpcKey, strlen(psSetting->pKey)) == 0)) {

            long lVal;
            switch (psSetting->eType) {
                case TYPE_LONG:
                    lVal = strtol(cpcValue, NULL, 10);
                    memcpy(psSetting->pvDst, &lVal, sizeof(long));
                    break;

                case TYPE_STRING:
                    if (strlen(cpcValue) > MAX_SETTINGS_LEN) {
                        return EXIT_FAILURE;
                    }

                    psSetting->pvDst = malloc(strlen(cpcValue));
                    memset(psSetting->pvDst, 0, iSize);
                    memcpy(psSetting->pvDst, cpcValue, strlen(cpcValue));

                    break;

                default:
                    return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

sSettingsStruct *settings_init(void) {
    return &sSettings;
}

int32_t settings_destroy(void) {
    return 0; //TODO

    // loop settings and free.
}

int32_t settings_load(const uint8_t *cpcSettingsFile) {
    struct INI *sIni = NULL;

    sIni = ini_open(cpcSettingsFile);
    if (!sIni) {
        return EXIT_FAILURE;
    }

    printf("INI file opened.\n");

    while (1) {
        const char *cpcBuf;
        char *pcSection;
        size_t SectionLen;

        int iRet = ini_next_section(sIni, &cpcBuf, &SectionLen);
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

            iRet = ini_read_pair(sIni, &cpcBuf, &KeyLen, &buf2, &ValueLen);
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

            // Make everything lower case
            for (int i = 0; pcSection[i]; i++) {
                pcSection[i] = tolower(pcSection[i]);
            }

            for (int i = 0; pcKey[i]; i++) {
                pcKey[i] = tolower(pcKey[i]);
            }

            if (parse_option(pcSection, pcKey, pcValue)) {
                printf("Error parsing option");
            }
        }
    }

    ini_close(sIni);
    show_settings();
    return EXIT_SUCCESS;


    error:
    ini_close(sIni);
    return EXIT_FAILURE;
}
