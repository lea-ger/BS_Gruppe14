#ifndef DYNHASHMAP_H
#define DYNHASHMAP_H

#include "shmalloc.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>


#define HASHMAP_INITIAL_CAPACITY 8


typedef struct HashmapItem {
    struct HashmapItem *nextBucketItem;
    void *value;
    char key[1];
} HashmapItem;


typedef struct {
    HashmapItem **table;
    int tableSize;
    int size;
} Hashmap;


Hashmap* hashmapCreate ();
Hashmap* hashmapCreateWithCapacity (int capacity);

Hashmap* hashmapCreateShm ();
Hashmap* hashmapCreateWithCapacityShm (int capacity);

void hashmapFree (Hashmap *hashmap, void(*freeValue)(void*));

size_t hashmapSize (const Hashmap *hashmap);
bool hashmapIsEmpty (const Hashmap *hashmap);
void hashmapClear (Hashmap* hashmap, void(*freeValue)(void*));

void* hashmapGetValue (const Hashmap *hashmap, const char* key);
const char* hashmapGetKey (const Hashmap *hashmap, const char* key);
bool hashmapContainsKey (const Hashmap *hashmap, const char* key);
void* hashmapAddItem (Hashmap *hashmap, const char* key, void* value);
void* hashmapPutItem (Hashmap *hashmap, const char* key, void* value);
void* hashmapRemoveItem (Hashmap *hashmap, const char* key);

HashmapItem* hashmapNextItem (Hashmap *hashmap, HashmapItem *previous);
void hashmapForEach (Hashmap *hashmap, void (*func)(const char* key, void* value));


#endif //DYNHASHMAP_H
