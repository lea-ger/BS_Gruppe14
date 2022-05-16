#ifndef SHMALLOC_H
#define SHMALLOC_H

#include "utils.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


void shminit (size_t capacity);
void shmcleanup ();

void* shmalloc (size_t size);
void* shmrealloc (void* ptr, size_t size);
void shmfree (void* ptr);
void* shmres (void* ptr);
void* shmdis (void* ptr);


static inline void* pass (void *ptr) { return ptr; };
static inline size_t alignSize (size_t size, size_t alignment) {
    return (size + alignment-1) & ~(alignment-1);
}

typedef struct {
    void* (*reserve)(size_t size);
    void* (*resize)(void *ptr, size_t size);
    void  (*release)(void *ptr);
    void* (*resolve)(void *ptr);
} Allocator;

extern const Allocator allocator[2];


#endif //SHMALLOC_H
