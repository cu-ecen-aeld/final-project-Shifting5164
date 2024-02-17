#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <ctype.h>

#include <ini.h>
#include <sds.h>

#include <banned.h>
#include <cew_settings.h>
#include <cew_logger.h>

static tsSSettings gsCurrSSettings = {0};

/* Type definitions for different types of settings */
typedef enum eSettingsType {
    TYPE_NONE,
    TYPE_LONG,
    TYPE_STRING,
} eSettingsType;

/* Layout of settings mapping in the config file vs the settings sutrcture */
typedef struct sOptionMapping {
    char *pSection;         // section of the ini
    char *pKey;             // config key in that section
    eSettingsType eType;    // type definition
    void *pvDst;            // destination memory
} tsOptionMapping;

/* Actual mapping for the file settings to memory */
static tsOptionMapping sKnownOptions[] = {
        {"general", "workers", TYPE_LONG,   &gsCurrSSettings.lWorkerThreads},
        {"general", "clients", TYPE_LONG,   &gsCurrSSettings.lMaxClientsPerThread},
        {"logging", "logfile", TYPE_STRING, &gsCurrSSettings.pcLogfile},
        {"logging", "level",   TYPE_LONG,   &gsCurrSSettings.lLogLevel},
        {NULL, NULL,           TYPE_NONE, NULL} // not needed, just for security
};

/* Write the final configuration after loading and parsing settings to the log. */
void settings_to_log(void) {
    tsOptionMapping *psSetting = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(tsOptionMapping);

    for (int32_t i = 0; i < iSize - 1; i++) {

        psSetting = &sKnownOptions[i];

        assert(psSetting != NULL);
        assert(psSetting->pSection != NULL);

        char **pcString = (char **) psSetting->pvDst;
        switch (psSetting->eType) {
            case TYPE_LONG:
                log_info("Setting %s:%s = %ld", psSetting->pSection, psSetting->pKey, *(long *) (psSetting->pvDst));
                break;

            case TYPE_STRING:
                log_info("Setting %s:%s = %s", psSetting->pSection, psSetting->pKey, *pcString);
                break;

            default:
                break;
        }
    }
}

/* Parse the options passed in section, with value. When the section and key are found in the mapping the
 * value will be copied to the memory. If no mapping is found, then its ignored.
 *
 * Return:
 * SET_EXIT_SUCCESS: when mapping is found, and value is copied
 * SET_NOTYPE : when no type is known
 * SET_NOMAP : when no mapping is found
 */
static int32_t parse_option(const sds cpcSection, const sds cpcKey, const sds cpcValue) {
    tsOptionMapping *psSetting = NULL;
    int32_t iSize = sizeof(sKnownOptions) / sizeof(tsOptionMapping);

    for (int32_t i = 0; i < iSize - 1; i++) {

        psSetting = &sKnownOptions[i];

        /* Make sure we don't run out of the mapping */
        assert(psSetting != NULL);
        assert(psSetting->pSection != NULL);

        if ((strncmp(psSetting->pSection, cpcSection, strlen(psSetting->pSection)) == 0) &&
            (strncmp(psSetting->pKey, cpcKey, strlen(psSetting->pKey)) == 0)) {

            long lVal;
            char **ppcStr = (char **) psSetting->pvDst;
            switch (psSetting->eType) {
                case TYPE_LONG:
                    lVal = strtol(cpcValue, NULL, 10);
                    memcpy(psSetting->pvDst, &lVal, sizeof(long)); //TODO
                    return SET_EXIT_SUCCESS;

                case TYPE_STRING:
                    /* When we are overwriting and old setting, then free old sds
                    and add new. */
                    if (*ppcStr != NULL) {
                        sdsfree(*ppcStr);
                    }

                    *ppcStr = sdsdup(cpcValue);
                    break;

                default:
                    return SET_NOTYPE;
            }
        }
    }

    return SET_NOMAP;
}

int32_t settings_load(const char *cpcSettingsFile) {
    struct INI *sIni = NULL;

    sIni = ini_open(cpcSettingsFile);
    if (!sIni) {
        return SET_NOFILE;
    }

    log_debug("INI file %s opened.", cpcSettingsFile);

    while (1) {
        const char *cpcBuf;
        sds Section;
        size_t SectionLen;

        int iRet = ini_next_section(sIni, &cpcBuf, &SectionLen);
        if (!iRet) {
            log_debug("End of file.");
            break;
        }

        if (iRet < 0) {
            log_debug("ERROR: code %i", iRet);
            goto error;
        }

        Section = sdsnewlen(cpcBuf, SectionLen);

        log_debug("Opening section: \'%s\'", Section);

        while (1) {
            const char *buf2;
            sds Key, Value;
            size_t KeyLen, ValueLen;

            iRet = ini_read_pair(sIni, &cpcBuf, &KeyLen, &buf2, &ValueLen);
            if (!iRet) {
                log_debug("No more data.");
                break;
            }

            if (iRet < 0) {
                log_error("ERROR: code %i", iRet);
                goto error;
            }

            Key = sdsnewlen(cpcBuf, KeyLen);
            Value = sdsnewlen(buf2, ValueLen);

            log_info("Reading Key: \'%s\' Value: \'%s\'", Key, Value);

            // Make everything lower case
            for (int i = 0; Section[i]; i++) {
                Section[i] = tolower(Section[i]);
            }

            for (int i = 0; Key[i]; i++) {
                Key[i] = tolower(Key[i]);
            }

            if (parse_option(Section, Key, Value)) {
                log_error("Error parsing option");
            }

            sdsfree(Key);
            sdsfree(Value);

        }

        sdsfree(Section);
    }

    ini_close(sIni);
    settings_to_log();
    return SET_EXIT_SUCCESS;


    error:
    ini_close(sIni);
    return SET_EXIT_FAILURE;
}

/*
 * Init the settings.
 */
int32_t settings_init(void) {
    return SET_EXIT_SUCCESS;
}

/*
 * Destroy the settings.
 */
int32_t settings_destroy(void) {
    if (gsCurrSSettings.pcLogfile != NULL) {
        sdsfree(gsCurrSSettings.pcLogfile);
        gsCurrSSettings.pcLogfile = NULL;
    }

    memset(&gsCurrSSettings, 0, sizeof(tsSSettings));

    return SET_EXIT_SUCCESS;

}

/* Return only a copy */
tsSSettings settings_get(void) {
    return gsCurrSSettings;
}

int32_t settings_set(tsSSettings sNewSettings) {
    gsCurrSSettings = sNewSettings;

    return SET_EXIT_SUCCESS;
}



