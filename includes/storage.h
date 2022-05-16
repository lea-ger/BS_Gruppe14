#ifndef SERVER_STORAGE_H
#define SERVER_STORAGE_H


#define FILE_BUFFER_SIZE PAGE_SIZE

#define STORAGE_MIN_ENTRIES 1000
#define STORAGE_MIN_SIZE 50000

#define STORAGE_FILE "../data.csv"


#include "utils.h"
#include "command.h"
#include "lock.h"
//#include "newsletter.h"

#include <stdio.h>
#include <sys/shm.h>


void eventCommandGet (Command *cmd);
void eventCommandPut (Command *cmd);
void eventCommandDel (Command *cmd);

void initModuleStorage (int snapshotInterval);
void freeModuleStorage ();


bool getStorageRecord (const char* key, String* value);
int putStorageRecord (const char* key, const char* value);
bool deleteStorageRecord (const char* key);

void getMultipleStorageRecords (const char* wildcardKey, Array* result);
void deleteMultipleStorageRecords (const char* wildcardKey, Array* result);

bool loadStorageFromFile ();
bool saveStorageToFile ();
bool determineFileStorageSize (size_t *records, size_t *dataSize);

void runSnapshotTimer (int interval);


#endif //SERVER_STORAGE_H
