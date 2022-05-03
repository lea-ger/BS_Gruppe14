#ifndef SERVER_STORAGE_H
#define SERVER_STORAGE_H

#include "utils.h"
#include "command.h"
#include "lock.h"

#include <stdio.h>
#include <sys/shm.h>


#define STORAGE_KEY_SIZE 64
#define STORAGE_VALUE_SIZE 256
#define STORAGE_ENTRY_SIZE 1000

#define STORAGE_SNAPSHOT_INTERVAL 60


static const char* storageFile = "../data.csv";


typedef struct {
    char key[STORAGE_KEY_SIZE];
    char value[STORAGE_VALUE_SIZE];
} Record;


void eventCommandGet (Command *cmd);
void eventCommandPut (Command *cmd);
void eventCommandDel (Command *cmd);

void initModulStorage (bool snapshotTimer);
void freeModulStorage ();

bool getStorageRecord (const char* key, String* value);
int putStorageRecord (const char* key, const char* value);
bool deleteStorageRecord (const char* key);

void getMultipleStorageRecords (const char* wildcardKey, Array* result);
void deleteMultipleStorageRecords (const char* wildcardKey, Array* result);

bool loadStorageFromFile ();
bool saveStorageToFile ();

void runSnapshotTimer ();


#endif //SERVER_STORAGE_H
