#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>


#define INITIAL_ARRAY_CAPACITY 4


typedef struct {
    void **cArr;
    size_t size;
    size_t capacity;
} Array;


Array* arrayCreate ();
Array* arrayCreateWithCapacity (size_t capacity);
Array* arrayCreateWithCArray (size_t size, void* cArr[]);
void arrayFree (Array *arr);
void arrayReserve (Array *arr, size_t capacity);
void arrayShrinkToFit (Array* arr);

size_t arraySize (const Array *arr);
bool arrayIsEmpty (const Array *arr);
void arrayClear (Array *arr);

void* arrayGetItem (const Array *arr, size_t index);
void arraySetItem (Array *arr, size_t index, void *item);
void arrayPushItem (Array *arr, void *item);
void* arrayPopItem (Array *arr);

void arrayShiftItem (Array *arr, size_t src, size_t dest);
void arrayInsertItem (Array *arr, size_t index, void *item);
void* arrayRemoveItem (Array *arr, size_t index);

void arrayForEach (Array *arr, void (*func)(void*));
void arrayInsertionSort (Array *arr, int (*compare)(void*, void*));
size_t arrayMakeUnique (Array *arr, int (*compare)(void*, void*));

void arrInsertionSort (void **arr, size_t size, int (*compare)(void*, void*));
size_t arrMakeUnique (void **arr, size_t size, int (*compare)(void*, void*));


#endif //DYNARRAY_H
