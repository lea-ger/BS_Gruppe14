#include "storage.h"


/*
 * In-memory Verwaltung der Datenbank
 *
 * Hält die Daten in einem Shared Memory Segment, bietet abgesicherte
 * Zugriffs-Funktionen darauf und läd/speichert die Daten aus/in eine Datei.
 * Leider nur ein statisches Array...
 *
 */


static int shmStorageSegmentId = 0;

static Record *storage = NULL;
static int *storageEndIndex = NULL;

const static char* keyDeletedMsg = "key_deleted";


void initModuleStorage (int snapshotInterval)
{
    registerCommandEntry("GET", 1, true, eventCommandGet);
    registerCommandEntry("PUT", 2, false, eventCommandPut);
    registerCommandEntry("DEL", 1, true, eventCommandDel);

    int storageSegmentSize = sizeof(Record) * STORAGE_ENTRY_SIZE + sizeof(int);

    // Erzeugt ein neues Shared-Memory-Segment
    shmStorageSegmentId = shmget(IPC_PRIVATE, storageSegmentSize, IPC_CREAT | SHM_R | SHM_W);
    if (shmStorageSegmentId == -1) {
        fatalError("initModuleStorage shmget");
    }
    printf("Storage shared memory segment created (Id %d).\n", shmStorageSegmentId);

    // Hängt das Shared-Memory-Segment in den lokalen Adressenraum ein
    // (Das Einhängen wird beim Erzeugen von Kind-Prozessen vererbt)
    storage = shmat(shmStorageSegmentId, NULL, 0);
    storageEndIndex = (int*)&storage[STORAGE_ENTRY_SIZE];
    memset(storage, 0, storageSegmentSize);

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

    // Hängt das Shared-Memory-Segment aus dem lokalen Adressenraum aus
    shmdt(storage);
    // Löscht das Shared-Memory-Segment
    shmctl(shmStorageSegmentId, IPC_RMID, NULL);
    printf("Storage shared memory segment deleted (Id %d).\n", shmStorageSegmentId);
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
 * Findet den Index eines Schlüssels im Storage, bei Fehlschlag -1.
 * Nicht gegen Race-Conditions gesichert.
 * NICHT AUSSERHALB EINES KRITISCHEN ABSCHNITTS AUFRUFEN!!!
 *
 * @param key - Suchschlüssel
 * @param accessTyp - Zugriffsart des Aufrufers
 * @return - Index des gefundenen Eintrags
 */
int findStorageRecord (const char* key)
{
    for (int i = 0; i < *storageEndIndex; i++) {
        if (strcmp(storage[i].key, key) == 0) {
            return i;
        }
        // Race-Conditions durch Trennung der Index-Iteration von der eigentlichen Operation.
        // Schlechte Idee.
        //leaveCriticalSection(accessType);
        //enterCriticalSection(accessType);
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
    enterCriticalSection(READ_ACCESS);

    int index = findStorageRecord(key);
    if (index != -1) {
        stringCopy(value, storage[index].value);

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

    // Sucht nach existierenden Einträgen
    int index = findStorageRecord(key);
    if (index != -1) {
        notifyAllObservers(NL_NOTIFICATION_PUT, index, key, value);

        strncpy(storage[index].value, value, STORAGE_VALUE_SIZE);

        leaveCriticalSection(WRITE_ACCESS);
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

        leaveCriticalSection(WRITE_ACCESS);
        return 2; // RECORD_NEW
    }

    leaveCriticalSection(WRITE_ACCESS);
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
    enterCriticalSection(WRITE_ACCESS);

    int index = findStorageRecord(key);
    if (index != -1) {
        notifyAllObservers(NL_NOTIFICATION_DEL, index, storage[index].key, keyDeletedMsg);

        *storage[index].key = '\0';

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

    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0' &&
                strMatchWildcard(storage[i].key, wildcardKey)) {
            responseRecordsAdd(result, storage[i].key, storage[i].value);
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

    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0' &&
                 strMatchWildcard(storage[i].key, wildcardKey)) {
            responseRecordsAdd(result, storage[i].key, keyDeletedMsg);

            notifyAllObservers(NL_NOTIFICATION_DEL, i, storage[i].key, keyDeletedMsg);

            strcpy(storage[i].key, "");
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

        strncpy(storage[*storageEndIndex].key, key, STORAGE_KEY_SIZE);
        strncpy(storage[*storageEndIndex].value, value, STORAGE_VALUE_SIZE);

        (*storageEndIndex)++;
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

    for (int i = 0; i < *storageEndIndex; i++) {
        if (*storage[i].key != '\0') {
            fprintf(file, "%s,%s\n", storage[i].key, storage[i].value);
        }
    }

    fclose(file);
    return true;
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
