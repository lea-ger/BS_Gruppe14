#include "storage.h"


typedef struct {
    char key[64];
    char value[256];
} Record;

// FIXME: Nur für Test-Zwecke (nur zum lesen geeignet)
static Record storage[1000];
static int storageSize = 0;


void loadStorageFile ()
{
    char buffer[512];
    FILE *file = fopen("../data.csv", "r");
    if (!file) return;
    while (fgets(buffer, sizeof(buffer), file)) {
        char* key = strtok(buffer,  ",");
        char* value = strtok(NULL, "\n");
        strncpy(storage[storageSize].key, key, 64);
        strncpy(storage[storageSize].value, value, 256);
        storageSize++;
    }
    fclose(file);
}


void initModulStorage ()
{
    registerCommandEntry("GET", 1, true, eventCommandGet);
    registerCommandEntry("PUT", 2, false, eventCommandPut);
    registerCommandEntry("DEL", 1, true, eventCommandDel);
    loadStorageFile();
}


void freeModulStorage ()
{
}


void eventCommandGet (Command *cmd)
{
    for (int i = 0; i < storageSize; i++) {
        if (strMatchWildcard(storage[i].key, cmd->key->cStr)) {
            responseRecordsAdd(cmd->responseRecords, storage[i].key, storage[i].value);
        }
    }
    if (cmd->responseRecords->size == 0) {
        stringCopy(cmd->responseMessage,  "KEY_NONEXISTENT");
    }
}


void eventCommandPut (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}


void eventCommandDel (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}
