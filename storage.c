#include "storage.h"


static int shmSegmentId = 0;

static Record *storage = NULL;
static int *storageEndIndex = NULL;


void initModulStorage ()
{
    registerCommandEntry("GET", 1, true, eventCommandGet);
    registerCommandEntry("PUT", 2, false, eventCommandPut);
    registerCommandEntry("DEL", 1, true, eventCommandDel);

    int segmentSize = sizeof(Record) * STORAGE_ENTRY_SIZE + sizeof(int);

    // Erzeugt ein neues Shared-Memory-Segment
    shmSegmentId = shmget(IPC_PRIVATE, segmentSize, IPC_CREAT | SHM_R | SHM_W);
    if (shmSegmentId == -1) {
        fatalError("shmget");
    }
    printf("Shared memory segment created (Id %d).\n", shmSegmentId);

    // Hängt das Shared-Memory-Segment in den lokalen Adressenraum ein
    // (Das Einhängen wird beim Erzeugen von Kind-Prozessen vererbt)
    storage = shmat(shmSegmentId, NULL, 0);
    storageEndIndex = (int*)&storage[STORAGE_ENTRY_SIZE];
    memset(storage, 0, segmentSize);

    if (loadStorageFromFile()) {
        printf("Storage data was loaded from file.\n");
    }
}


void freeModulStorage ()
{
    if (saveStorageToFile()) {
        printf("Storage data saved to file.\n");
    }

    // Hängt das Shared-Memory-Segment aus dem lokalen Adressenraum aus
    shmdt(storage);
    // Löscht das Shared-Memory-Segment
    shmctl(shmSegmentId, IPC_RMID, NULL);
    printf("Shared memory segment deleted (Id %d).\n", shmSegmentId);
}


void eventCommandGet (Command *cmd)
{
    if (stringMatchAnyChar(cmd->key, "*?", STR_MATCH_NOGROUP) != -1) {
        getMultipleStorageRecords(cmd->key->cStr, cmd->responseRecords);
    }
    else {
        String *value = stringCreate("");
        if (getStorageRecord(cmd->key->cStr, value)) {
            responseRecordsAdd(cmd->responseRecords, cmd->key->cStr, value->cStr);
        }
        stringFree(value);
    }

    if (cmd->responseRecords->size == 0) {
        stringCopy(cmd->responseMessage,  "KEY_NONEXISTENT");
    }
}


void eventCommandPut (Command *cmd)
{
    int response = putStorageRecord(cmd->key->cStr, cmd->value->cStr);
    if (response > 0) {
        responseRecordsAdd(cmd->responseRecords, cmd->key->cStr, cmd->value->cStr);
        if (response == 1) {
            stringCopy(cmd->responseMessage,  "RECORD_OVERWRITTEN");
        }
        else if (response == 2) {
            stringCopy(cmd->responseMessage,  "RECORD_NEW");
        }
    }
    else {
        stringCopy(cmd->responseMessage,  "STORAGE_FULL");
    }
}


void eventCommandDel (Command *cmd)
{
    if (stringMatchAnyChar(cmd->key, "*?", STR_MATCH_NOGROUP) != -1) {
        deleteMultipleStorageRecords(cmd->key->cStr, cmd->responseRecords);
    }
    else {
        if (deleteStorageRecord (cmd->key->cStr)) {
            responseRecordsAdd(cmd->responseRecords, cmd->key->cStr, "KEY_DELETED");
        }
    }

    if (cmd->responseRecords->size == 0) {
        stringCopy(cmd->responseMessage,  "KEY_NONEXISTENT");
    }
}


/**
 * Findet den Index eines Schlüssels im Storage, bei Fehlschlag -1.
 * Nicht gegen Race-Conditions gesichert weil nur für die Nutzung
 * innerhalb der Get/Put/Delete Funktionen gedacht.
 *
 * NICHT AUSSERHALB EINES KRITISCHEN ABSCHNITTS AUFRUFEN!!!
 *
 * @param key - Suchschlüssel
 * @return - Index des gefundenen Eintrags
 */
int findStorageRecord (const char* key)
{
    for (int i = 0; i < *storageEndIndex; i++) {
        if (strcmp(storage[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}


/**
 * Sucht einen Schlüssel im Storage und kopiert dessen Wert nach "value".
 *
 * @param key - Suchschlüssel
 * @param value - Wert des gefundenen Eintrags
 */
bool getStorageRecord (const char* key, String* value)
{
    int index = findStorageRecord(key);
    if (index != -1) {
        stringCopy(value, storage[index].value);
        return true;
    }
    return false;
}


/**
 * Macht einen Eintrag ins Storage. Hat folgende Rückgabewerte:
 * - 1 Der Eintrag existierte schon, sein Wert wurde verändert
 * - 2 Ein neuer Eintrag wurde angelegt
 * - 0 Es war kein freier Platz verfügbar.
 *
 * @param key - Schlüssel des einzufügenden Eintrags
 * @param value - Wert des einzufügenden Eintrags
 */
int putStorageRecord (const char* key, const char* value)
{
    // Sucht nach existierenden Einträgen
    int index = findStorageRecord(key);
    if (index != -1) {
        strncpy(storage[index].value, value, STORAGE_VALUE_SIZE);
        return 1; // RECORD_OVERWRITTEN
    }

    // Sucht nach dem ersten freien Platz oder einem Platz am Ende
    index = findStorageRecord("");
    if (index == -1 && *storageEndIndex < STORAGE_ENTRY_SIZE) {
        index = (*storageEndIndex)++;
    }
    if (index != -1) {
        strncpy(storage[index].key, key, STORAGE_KEY_SIZE);
        strncpy(storage[index].value, value, STORAGE_VALUE_SIZE);
        return 2; // RECORD_NEW
    }

    return 0; // STORAGE_FULL
}


/**
 * Entfernt einen Eintrag aus dem Storage indem es seinen Schlüssel mit einem
 * Leerstring ersetzt (freier Platz).
 *
 * @param key - Schlüssel des zu löschenden Eintrags
 */
bool deleteStorageRecord (const char* key)
{
    int index = findStorageRecord(key);
    if (index != -1) {
        *storage[index].key = '\0';
        return true;
    }
    return false;
}


/**
 * Vergleicht alle Einträge mit dem Wildcard-Suchschlüssel und fügt alle Treffer
 * dem Ergebnis-Array hinzu.
 *
 * @param wildcardKey - Wildcard-Suchschlüssel
 * @param result - Ergebnis-Array
 */
void getMultipleStorageRecords (const char* wildcardKey, Array* result)
{
    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0' &&
                strMatchWildcard(storage[i].key, wildcardKey)) {
            responseRecordsAdd(result, storage[i].key, storage[i].value);
        }
    }
}


/**
 * Vergleicht alle Einträge mit dem Wildcard-Suchschlüssel und fügt alle Treffer
 * dem Ergebnis-Array hinzu. Entfernt alle gefundenen Einträge aus dem Storage
 * indem es ihre Schlüssel mit Leerstrings ersetzt (freier Platz).
 *
 * @param wildcardKey - Wildcard-Suchschlüssel
 * @param result - Ergebnis-Array
 */
void deleteMultipleStorageRecords (const char* wildcardKey, Array* result)
{
    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0' &&
                strMatchWildcard(storage[i].key, wildcardKey)) {
            responseRecordsAdd(result, storage[i].key, "KEY_DELETED");
            strcpy(storage[i].key, "");
        }
    }
}


/**
 * Befüllt das Storage mit den Einträgen aus der "storageFile"-Datei.
 * Zeilenweise Einträge, Schlüssel und Wert kommasepariert.
 *
 */
bool loadStorageFromFile ()
{
    char lineBuffer[STORAGE_KEY_SIZE + STORAGE_KEY_SIZE + 4];

    FILE *file = fopen(storageFile, "r");
    if (file == NULL) {
        return false;
    }

    while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
        const char* key = strtok(lineBuffer,  ",");
        const char* value = strtok(NULL, "\n");

        strncpy(storage[*storageEndIndex].key, key, STORAGE_KEY_SIZE);
        strncpy(storage[*storageEndIndex].value, value, STORAGE_KEY_SIZE);

        (*storageEndIndex)++;
    }

    fclose(file);
    return true;
}


/**
 * Speichert den Inhalt des Storage in die "storageFile"-Datei.
 * Zeilenweise Einträge, Schlüssel und Wert kommasepariert.
 *
 */
bool saveStorageToFile ()
{
    FILE *file = fopen(storageFile, "w+");
    if (file == NULL) {
        return false;
    }

    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0') {
            fprintf(file, "%s,%s\n", storage[i].key, storage[i].value);
        }
    }

    fclose(file);
    return true;
}

