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
#include <sdsalloc.h>

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

        char **pcString = (char *)psSetting->pvDst;
        switch (psSetting->eType) {
            case TYPE_LONG:
                printf("Setting %s:%s = %ld\n", psSetting->pSection, psSetting->pKey, *(long *) (psSetting->pvDst));
                break;

            case TYPE_STRING:
                printf("Setting %s:%s = %s\n", psSetting->pSection, psSetting->pKey, *pcString );
                break;

            default:
                break;
        }
    }

    printf("pcLogfile:%s\n", sSettings.pcLogfile);
    printf("lLogLevel:%d\n", sSettings.lLogLevel);
    printf("lMaxClientsPerThread:%d\n", sSettings.lMaxClientsPerThread);
    printf("lWorkerThreads:%d\n", sSettings.lWorkerThreads);


}

static int32_t parse_option(const sds cpcSection, const sds cpcKey, const sds cpcValue) {
    tsOptionMapping *psSetting = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(tsOptionMapping);

    for (int i = 0; i < iSize - 1; i++) {

        psSetting = &sKnownOptions[i];

        assert(psSetting != NULL);
        assert(psSetting->pSection != NULL);

        if ((strncmp(psSetting->pSection, cpcSection, strlen(psSetting->pSection)) == 0) &&
            (strncmp(psSetting->pKey, cpcKey, strlen(psSetting->pKey)) == 0)) {

            long lVal;
            char **pStr = (char *)psSetting->pvDst;
            switch (psSetting->eType) {
                case TYPE_LONG:
                    lVal = strtol(cpcValue, NULL, 10);
                    memcpy(psSetting->pvDst, &lVal, sizeof(long));
                    break;

                case TYPE_STRING:                    

                    /* When we are overwriting, then free old sds 
                    and add new. */
                    if (*pStr != NULL){
                        sdsfree(*pStr);
                    }
                    
                    *pStr = sdsdup(cpcValue);            
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
    if (sSettings.pcLogfile != NULL) {
        sdsfree(sSettings.pcLogfile);
    }

    memset(&sSettings, 0 ,sizeof(sSettingsStruct));

    return EXIT_SUCCESS;

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
        sds Section;
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

        Section = sdsnewlen(cpcBuf, SectionLen);

        printf("Opening section: \'%s\'\n", Section);

        while (1) {
            const char *buf2;
            sds Key, Value;
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

            Key = sdsnewlen(cpcBuf, KeyLen);
            Value = sdsnewlen(buf2, ValueLen);

            printf("Reading Key: \'%s\' Value: \'%s\'\n", Key, Value);

            // Make everything lower case
            for (int i = 0; Section[i]; i++) {
                Section[i] = tolower(Section[i]);
            }

            for (int i = 0; Key[i]; i++) {
                Key[i] = tolower(Key[i]);
            }

            if (parse_option(Section, Key, Value)) {
                printf("Error parsing option");
            }

            sdsfree(Key);
            sdsfree(Value);

        }

        sdsfree(Section);
    }

    ini_close(sIni);
    show_settings();
    return EXIT_SUCCESS;


    error:
    ini_close(sIni);
    return EXIT_FAILURE;
}
