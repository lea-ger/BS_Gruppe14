#include "storage.h"


/*
 * In-memory Verwaltung der Datenbank
 *
 * Hält die Daten in einem Shared Memory Segment, bietet abgesicherte
 * Zugriffs-Funktionen darauf und läd/speichert die Daten aus/in eine Datei.
 * Leider nur ein statisches Array...
 *
 */


static Hashmap *storage = NULL;

const static char* keyDeletedMsg = "key_deleted";


void initModuleStorage (int snapshotInterval)
{
    registerCommandEntry("GET", 1, true, eventCommandGet);
    registerCommandEntry("PUT", 2, false, eventCommandPut);
    registerCommandEntry("DEL", 1, true, eventCommandDel);

    size_t initialStorageEntries = STORAGE_MIN_ENTRIES;
    size_t initialStorageSize = STORAGE_MIN_SIZE;

    if (determineFileStorageSize(&initialStorageEntries, &initialStorageSize)) {
        initialStorageEntries *= 2;
        initialStorageSize *= 4;
        if (initialStorageEntries < STORAGE_MIN_ENTRIES) initialStorageEntries = STORAGE_MIN_ENTRIES;
        if (initialStorageSize < STORAGE_MIN_SIZE) initialStorageSize = STORAGE_MIN_SIZE;
    }

    shminit(initialStorageSize);

    storage = hashmapCreateWithCapacityShm(initialStorageEntries);

    if (loadStorageFromFile()) {
        printf("Storage data was loaded from file.\n");
    }

    if (snapshotInterval > 0 && fork() == 0) {
        prctl(PR_SET_NAME, (unsigned long)"kvsvr(snapshot)");
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        runSnapshotTimer(snapshotInterval);

        exit(EXIT_SUCCESS);
    }
}


void freeModuleStorage ()
{
    if (saveStorageToFile()) {
        printf("Storage data saved to file.\n");
    }

    shmcleanup();
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
        stringCopy(cmd->responseMessage,  "key_nonexistent");
    }
}


void eventCommandPut (Command *cmd)
{
    int response = putStorageRecord(cmd->key->cStr, cmd->value->cStr);
    const char *message = "storage_full"; // response == 0

    if (response > 0) {
        responseRecordsAdd(cmd->responseRecords, cmd->key->cStr, cmd->value->cStr);

        switch (response) {
            case 1: message = "record_overwritten"; break;
            case 2: message = "record_new"; break;
            default: break;
        }
    }

    stringCopy(cmd->responseMessage, message);
}


void eventCommandDel (Command *cmd)
{
    if (stringMatchAnyChar(cmd->key, "*?", STR_MATCH_NOGROUP) != -1) {
        deleteMultipleStorageRecords(cmd->key->cStr, cmd->responseRecords);
    }
    else if (deleteStorageRecord(cmd->key->cStr)) {
        responseRecordsAdd(cmd->responseRecords, cmd->key->cStr, keyDeletedMsg);
    }

    if (cmd->responseRecords->size == 0) {
        stringCopy(cmd->responseMessage,  "key_nonexistent");
    }
}


/**
 * Sucht einen Schlüssel im Storage und kopiert dessen Wert nach "value".
 *
 * @param key - Suchschlüssel
 * @param value - Wert des gefundenen Eintrags
 */
bool getStorageRecord (const char* key, String* value)
{
    enterCriticalSection(READ_ACCESS);

    char *recordValue = hashmapGetValue(shmres(storage), key);
    if (recordValue != NULL) {
        stringCopy(value, recordValue);

        leaveCriticalSection(READ_ACCESS);
        return true;
    }

    leaveCriticalSection(READ_ACCESS);
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
    enterCriticalSection(WRITE_ACCESS);

    char *recordValue = shmalloc(strlen(value) + 1);
    strcpy(shmres(recordValue), value);
    char* removedValue = hashmapPutItem(shmres(storage), key, recordValue);

    if (removedValue != NULL) {
        //notifyAllObservers(NL_NOTIFICATION_PUT, key, value);
        shmfree(removedValue);
        leaveCriticalSection(WRITE_ACCESS);
        return 1; // RECORD_OVERWRITTEN
    }

    leaveCriticalSection(WRITE_ACCESS);
    return 2; // RECORD_NEW
}


/**
 * Entfernt einen Eintrag aus dem Storage indem es seinen Schlüssel mit einem
 * Leerstring ersetzt (freier Platz).
 *
 * @param key - Schlüssel des zu löschenden Eintrags
 */
bool deleteStorageRecord (const char* key)
{
    enterCriticalSection(WRITE_ACCESS);

    char *recordValue = hashmapRemoveItem(shmres(storage), key);
    if (recordValue != NULL) {
        //notifyAllObservers(NL_NOTIFICATION_DEL, key, keyDeletedMsg);
        shmfree(recordValue);
        leaveCriticalSection(WRITE_ACCESS);
        return true;
    }

    leaveCriticalSection(WRITE_ACCESS);
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
    enterCriticalSection(READ_ACCESS);

    for (HashmapItem *record = NULL; (record = hashmapNextItem(shmres(storage), record)) != NULL;) {
        if (strMatchWildcard(record->key, wildcardKey)) {
            char *recordValue = shmres(record->value);
            responseRecordsAdd(result, record->key, recordValue);
        }
    }

    leaveCriticalSection(READ_ACCESS);
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
    enterCriticalSection(WRITE_ACCESS);

    for (HashmapItem *record = NULL; (record = hashmapNextItem(shmres(storage), record)) != NULL;) {
        if (strMatchWildcard(record->key, wildcardKey)) {
            //notifyAllObservers(NL_NOTIFICATION_DEL, record->key, keyDeletedMsg);
            shmfree(shmres(record->value));
            responseRecordsAdd(result, record->key, keyDeletedMsg);
        }
    }

    leaveCriticalSection(WRITE_ACCESS);
}


/**
 * Befüllt das Storage mit den Einträgen aus der "STORAGE_FILE"-Datei.
 * Zeilenweise Einträge, Schlüssel und Wert kommasepariert.
 *
 */
bool loadStorageFromFile ()
{
    char lineBuffer[FILE_BUFFER_SIZE];

    FILE *file = fopen(STORAGE_FILE, "r");
    if (file == NULL) {
        return false;
    }

    while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
        const char* key = strtok(lineBuffer,  ",");
        const char* value = strtok(NULL, "\n");

        if (key == NULL || value == NULL) continue;

        char *recordValue = shmalloc(strlen(value) + 1);
        strcpy(shmres(recordValue), value);

        if (hashmapAddItem(shmres(storage), key, recordValue) != NULL) {
            shmfree(shmres(recordValue));
        }
    }

    fclose(file);
    return true;
}


/**
 * Speichert den Inhalt des Storage in die "STORAGE_FILE"-Datei.
 * Zeilenweise Einträge, Schlüssel und Wert kommasepariert.
 *
 */
bool saveStorageToFile ()
{
    FILE *file = fopen(STORAGE_FILE, "w+");
    if (file == NULL) {
        return false;
    }

    for (HashmapItem *record = NULL; (record = hashmapNextItem(shmres(storage), record)) != NULL;) {
        char *recordValue = shmres(record->value);
        fprintf(file, "%s,%s\n", record->key, recordValue);
    }

    fclose(file);
    return true;
}


bool determineFileStorageSize (size_t *records, size_t *dataSize)
{
    static char lineBuffer[FILE_BUFFER_SIZE];
    *records = 0;
    *dataSize = 0;

    // Bestimme die Anzahl der Einträge und Datenmenge der Datei
    FILE *file = fopen(STORAGE_FILE, "r");
    if (file != NULL) {
        while (fgets(lineBuffer, sizeof(char)*FILE_BUFFER_SIZE, file)) {
            (*records)++;
        }
        fseek(file, 0, SEEK_END);
        *dataSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        fclose(file);
        return true;
    }
    return false;
}


void eventSnapshotTimer ()
{
    clock_t clockTime = clock();

    enterCriticalSection(READ_ACCESS);
    saveStorageToFile();
    leaveCriticalSection(READ_ACCESS);

    double time_taken = (double)(clock() - clockTime) / CLOCKS_PER_SEC;
    printf("Storage saved by Snapshot-Timer (time taken: %f sec).\n", time_taken);
    fflush(stdout);
}


void runSnapshotTimer (int interval)
{
    signal(SIGALRM, eventSnapshotTimer);
    for (;;) {
        alarm(interval);
        pause();
    }
}
