#include "dynHashmap.h"


/*
 * Hashmap
 *
 * Eine unsortierte Datenstruktur mit konstanten Laufzeiten.
 * Verwendet die FNV-1a Hash-Funktion für den Key-String und
 * speichert Kollisionen als Liste im Bucket.
 *
 */


static inline const Allocator* hashmapAllocator (const Hashmap *hashmap)
{
    return &allocator[hashmap->tableSize & 1];
}
static inline size_t hashmapTableSize (const Hashmap *hashmap)
{
    return hashmap->tableSize & ~1UL;
}
static inline HashmapItem** hashmapTable (const Hashmap *hashmap)
{
    return hashmapAllocator(hashmap)->resolve(hashmap->table);
}


// FNV-1a Hash-Funktion (32bit)
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
uint32_t hash32 (const char* str)
{
    uint32_t hash = 2166136261;
    for (; *str; str++) {
        hash ^= *str;
        hash *= 16777619;
    }
    return hash;
}


HashmapItem** hashmapFindItem (const Hashmap *hashmap, const char* key)
{
    uint32_t hash = hash32(key);
    int index = (int)(hash & hashmapTableSize(hashmap) - 1);

    const Allocator *alloc = hashmapAllocator(hashmap);
    HashmapItem **table = hashmapTable(hashmap);

    HashmapItem *item = table[index];
    HashmapItem **itemPointer = &table[index];

    while (item != NULL) {
        item = alloc->resolve(item);
        if (strcmp(item->key, key) == 0) return itemPointer;
        itemPointer = &(*item).nextBucketItem;
        item = item->nextBucketItem;
    }

    return itemPointer;
}


void hashmapAdjustTableSize (Hashmap *hashmap)
{
    const Allocator *alloc = hashmapAllocator(hashmap);

    HashmapItem **oldTable = hashmapTable(hashmap);
    size_t oldTableSize = hashmapTableSize(hashmap);
    size_t tableSize = oldTableSize * 2;

    hashmap->tableSize = tableSize | (hashmap->tableSize & 1);
    hashmap->table = alloc->reserve(sizeof(HashmapItem*) * tableSize);
    HashmapItem **table = hashmapTable(hashmap);
    memset(table, 0, sizeof(HashmapItem*) * tableSize);

    for (int i = 0; i < oldTableSize; i++) {
        HashmapItem *item = oldTable[i];
        while (item != NULL) {
            HashmapItem *itemPtr = alloc->resolve(item);
            HashmapItem **itemPointer = hashmapFindItem(hashmap, itemPtr->key);
            *itemPointer = item;

            HashmapItem *nextItem = itemPtr->nextBucketItem;
            itemPtr->nextBucketItem = NULL;
            item = nextItem;
        }
    }

    alloc->release(oldTable);
}


Hashmap* _hashmapCreateWithCapacity (int capacity, bool shared)
{
    if (capacity < HASHMAP_INITIAL_CAPACITY) capacity = HASHMAP_INITIAL_CAPACITY;
    int initialTableSize = (int)pow(2, ceil(log(capacity) / log(2)));

    const Allocator *alloc = &allocator[shared];

    Hashmap *hashmap = alloc->reserve(sizeof(Hashmap));
    Hashmap *hashmapPtr = alloc->resolve(hashmap);

    hashmapPtr->table = alloc->reserve(sizeof(HashmapItem*) * initialTableSize);
    HashmapItem *tablePtr = alloc->resolve(hashmapPtr->table);

    memset(tablePtr, 0, sizeof(HashmapItem*) * initialTableSize);

    hashmapPtr->tableSize = initialTableSize | shared;
    hashmapPtr->size = 0;

    return hashmap;
}


Hashmap* hashmapCreateWithCapacity (int capacity)
{
    _hashmapCreateWithCapacity(capacity, false);
}


Hashmap* hashmapCreateWithCapacityShm (int capacity)
{
    _hashmapCreateWithCapacity(capacity, true);
}


Hashmap* hashmapCreate ()
{
    return _hashmapCreateWithCapacity(HASHMAP_INITIAL_CAPACITY, false);
}


Hashmap* hashmapCreateShm ()
{
    return _hashmapCreateWithCapacity(HASHMAP_INITIAL_CAPACITY, true);
}


void hashmapFree (Hashmap* hashmap, void(*freeValue)(void*))
{
    const Allocator *alloc = hashmapAllocator(hashmap);

    hashmapClear(hashmap, freeValue);

    alloc->release(hashmapTable(hashmap));
    alloc->release(hashmap);
}


/**
 * Gibt die Anzahl der Items zurück.
 *
 * @param hashmap - Zielobjekt
 * @return Anzahl der Items
 */
size_t hashmapSize (const Hashmap *hashmap)
{
    return hashmap->size;
}


/**
 * Wahr wenn das Hashmap keine Items beinhaltet.
 *
 * @param hashmap - Zielobjekt
 */
bool hashmapIsEmpty (const Hashmap *hashmap)
{
    return hashmap->size == 0;
}


void hashmapClear (Hashmap* hashmap, void(*freeValue)(void*))
{
    const Allocator *alloc = hashmapAllocator(hashmap);
    HashmapItem **table = hashmapTable(hashmap);
    size_t tableSize = hashmapTableSize(hashmap);

    for (int i = 0; i < tableSize; i++) {
        HashmapItem *item = table[i];
        while (item != NULL) {
            item = alloc->resolve(item);
            HashmapItem *nextItem = item->nextBucketItem;
            if (freeValue != NULL) freeValue(alloc->resolve(item->value));
            alloc->release(item);
            item = nextItem;
        }
        table[i] = NULL;
    }

    hashmap->size = 0;
}


void* hashmapGetValue (const Hashmap *hashmap, const char* key)
{
    void* (*resolve)(void*) = hashmapAllocator(hashmap)->resolve;
    HashmapItem **item = hashmapFindItem(hashmap, key);
    if (*item == NULL) return NULL;
    return resolve(((HashmapItem*)resolve(*item))->value);
}


const char* hashmapGetKey (const Hashmap *hashmap, const char* key)
{
    HashmapItem **item = hashmapFindItem(hashmap, key);
    if (*item == NULL) return NULL;
    return ((HashmapItem*)hashmapAllocator(hashmap)->resolve(*item))->key;
}


bool hashmapContainsKey (const Hashmap *hashmap, const char* key)
{
    HashmapItem **item = hashmapFindItem(hashmap, key);
    return (*item != NULL);
}


static inline void* hashmapInsertItem (Hashmap *hashmap, const char* key, void* value, bool overwrite)
{
    const Allocator *alloc = hashmapAllocator(hashmap);

    HashmapItem **item = hashmapFindItem(hashmap, key);
    HashmapItem *itemPtr = alloc->resolve(*item);

    if (*item != NULL) {
        if (overwrite) {
            void *overwrittenValue = itemPtr->value;
            itemPtr->value = value;
            return alloc->resolve(overwrittenValue);
        }
        else {
            return alloc->resolve(itemPtr->value);
        }
    }

    HashmapItem *newItem = alloc->reserve(sizeof(HashmapItem) + strlen(key));
    HashmapItem *newItemPtr = alloc->resolve(newItem);

    newItemPtr->nextBucketItem = NULL;
    newItemPtr->value = value;
    strcpy(newItemPtr->key, key);
    *item = newItem;

    hashmap->size++;

    if (hashmap->size > hashmapTableSize(hashmap)) hashmapAdjustTableSize(hashmap);

    return NULL;
}


void* hashmapAddItem (Hashmap *hashmap, const char* key, void* value)
{
    return hashmapInsertItem(hashmap, key, value, false);
}


void* hashmapPutItem (Hashmap *hashmap, const char* key, void* value)
{
    return hashmapInsertItem(hashmap, key, value, true);
}


void* hashmapRemoveItem (Hashmap *hashmap, const char* key)
{
    HashmapItem **item = hashmapFindItem(hashmap, key);
    if (*item == NULL) return NULL;

    const Allocator *alloc = hashmapAllocator(hashmap);

    HashmapItem *removedItem = alloc->resolve(*item);
    void *removedValue = alloc->resolve(removedItem->value);
    *item = removedItem->nextBucketItem;

    hashmap->size--;

    alloc->release(removedItem);

    return removedValue;
}


HashmapItem* hashmapNextItem (Hashmap *hashmap, HashmapItem *previous)
{
    static int index = 0;

    if (previous == NULL) {
        index = 0;
    }
    else if (previous->nextBucketItem != NULL) {
        return hashmapAllocator(hashmap)->resolve(previous->nextBucketItem);
    }

    HashmapItem **table = hashmapTable(hashmap);
    size_t tableSize = hashmapTableSize(hashmap);

    while (index < tableSize) {
        if (table[index] != NULL) {
            return hashmapAllocator(hashmap)->resolve(table[index++]);
        }
        index++;
    }

    return NULL;
}


void hashmapForEach (Hashmap *hashmap, void (*func)(const char* key, void* value))
{
    const Allocator *alloc = hashmapAllocator(hashmap);

    HashmapItem **table = hashmapTable(hashmap);
    size_t tableSize = hashmapTableSize(hashmap);

    for (int i = 0; i < tableSize; i++) {
        HashmapItem *item = table[i];
        while (item != NULL) {
            HashmapItem *itemPtr = alloc->resolve(item);
            func(itemPtr->key, alloc->resolve(itemPtr->value));
            item = item->nextBucketItem;
        }
    }
}

