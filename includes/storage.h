#ifndef SERVER_STORAGE_H
#define SERVER_STORAGE_H


#define FILE_BUFFER_SIZE PAGE_SIZE

#define STORAGE_KEY_SIZE 64
#define STORAGE_VALUE_SIZE 256
#define STORAGE_ENTRY_SIZE 1024

#define STORAGE_FILE "../data.csv"


#include "utils.h"
#include "command.h"
#include "lock.h"
#include "newsletter.h"

#include <stdio.h>
#include <sys/shm.h>


typedef struct {
    char key[STORAGE_KEY_SIZE];
    char value[STORAGE_VALUE_SIZE];
} Record;


void eventCommandGet (Command *cmd);
void eventCommandPut (Command *cmd);
void eventCommandDel (Command *cmd);
void eventCommandCount (Command *cmd);

void initModuleStorage (int snapshotInterval);
void freeModuleStorage ();

int findStorageRecord (const char* key);

bool getStorageRecord (const char* key, String* value);
int putStorageRecord (const char* key, const char* value);
bool deleteStorageRecord (const char* key);

void getMultipleStorageRecords (const char* wildcardKey, Array* result);
void deleteMultipleStorageRecords (const char* wildcardKey, Array* result);

bool loadStorageFromFile ();
bool saveStorageToFile ();

void runSnapshotTimer (int interval);


#endif //SERVER_STORAGE_H
