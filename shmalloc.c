#include "shmalloc.h"


#define BLOCK_HEADER_SIZE sizeof(size_t)
#define CHUNK_SIZE (BLOCK_HEADER_SIZE * 2)

typedef unsigned char byte;

typedef struct {
    int dataMemorySegmentId;
} UpdateMemorySegment;

typedef struct {
    void* firstFreeBlock;
    void* stopBlock;
} DataMemorySegmentHeader;

typedef struct Block {
    union {
        size_t size;
        size_t previousBlockInUse:1;
    };
    struct Block* previousFreeBlock;
    struct Block* nextFreeBlock;
} Block;


const Allocator allocator[2] = {
        {malloc, realloc, free, pass},
        {shmalloc, shmrealloc, shmfree, shmres}
};


static inline size_t blockSize (Block* blockPtr)
    { return blockPtr->size & ~1UL; }
static inline size_t* blockFooter (Block* blockPtr)
    { return (size_t*)(((byte*)blockPtr) + blockSize(blockPtr) - sizeof(size_t)); }
static inline Block* previousBlock (Block* blockPtr)
    { return (Block*)(((byte*)blockPtr) -  ((size_t*)blockPtr)[-1]); }
static inline Block* nextBlock (Block* blockPtr)
    { return (Block*)(((byte*)blockPtr) + blockSize(blockPtr)); }
static inline bool blockInUse (Block *blockPtr)
    { return nextBlock(blockPtr)->previousBlockInUse; }


void placeStopBlock (size_t dataSegmentSize);
void addBlockToFreeList (Block *blockPtr);
void removeBlockFromFreeList (Block *blockPtr);
Block* coalesceFreeBlocks (Block *blockPtr);
Block* findFittingFreeBlock (size_t size);
void* placeBlock (Block *blockPtr, size_t size);
Block* extendDataMemorySegment (size_t size);
void reattachDataMemorySegment ();


static int updateMemorySegmentId = 0;
static int dataMemorySegmentId = 0;
static UpdateMemorySegment *updateMemorySegment = NULL;
DataMemorySegmentHeader *dataMemorySegment = NULL;

static int updateProcessGroup = 0;


void placeStopBlock (size_t dataSegmentSize)
{
    dataMemorySegment->stopBlock = (void*)dataSegmentSize - CHUNK_SIZE;

    Block *stopBlock = shmres(dataMemorySegment->stopBlock);
    stopBlock->size = BLOCK_HEADER_SIZE;
    stopBlock->previousBlockInUse = 0; // FreeBlock ist verfügbar
    nextBlock(stopBlock)->size = 0;
    nextBlock(stopBlock)->previousBlockInUse = 1; // StopBlock ist belegt
}


void addBlockToFreeList (Block *blockPtr)
{
    Block *firstFreeBlock = dataMemorySegment->firstFreeBlock;

    blockPtr->previousFreeBlock = NULL;
    blockPtr->nextFreeBlock = firstFreeBlock;

    if (firstFreeBlock != NULL) {
        Block *firstFreeBlockPtr = shmres(firstFreeBlock);
        firstFreeBlockPtr->previousFreeBlock = shmdis(blockPtr);
    }

    dataMemorySegment->firstFreeBlock = shmdis(blockPtr);
}


void removeBlockFromFreeList (Block *blockPtr)
{
    if (blockPtr->previousFreeBlock != NULL) {
        Block *previousFreeBlockPtr = shmres(blockPtr->previousFreeBlock);
        previousFreeBlockPtr->nextFreeBlock = blockPtr->nextFreeBlock;
    }
    else {
        dataMemorySegment->firstFreeBlock = blockPtr->nextFreeBlock;
    }

    if (blockPtr->nextFreeBlock != NULL) {
        Block *nextFreeBlockPtr = shmres(blockPtr->nextFreeBlock);
        nextFreeBlockPtr->previousFreeBlock = blockPtr->previousFreeBlock;
    }
}


Block* coalesceFreeBlocks (Block *blockPtr)
{
    bool previousBlockInUse = blockPtr->previousBlockInUse;
    bool nextBlockInUse = blockInUse(nextBlock(blockPtr));

    if (previousBlockInUse && nextBlockInUse) {
        return blockPtr;
    }

    if (!previousBlockInUse) {
        Block *previousBlockPtr = previousBlock(blockPtr);
        removeBlockFromFreeList(blockPtr);

        previousBlockPtr->size += blockPtr->size;
        *blockFooter(previousBlockPtr) = blockSize(previousBlockPtr);

        blockPtr = previousBlockPtr;
    }

    if (!nextBlockInUse) {
        Block *nextBlockPtr = nextBlock(blockPtr);
        removeBlockFromFreeList(nextBlockPtr);

        blockPtr->size += nextBlockPtr->size;
        *blockFooter(blockPtr) = blockSize(blockPtr);
    }

    return blockPtr;
}


Block* findFittingFreeBlock (size_t size)
{
    Block *freeBlock = dataMemorySegment->firstFreeBlock;

    while (freeBlock != NULL) {
        Block *freeBlockPtr = shmres(freeBlock);
        if (blockSize(freeBlockPtr) >= size) {
            return freeBlockPtr;
        }
        freeBlock = freeBlockPtr->nextFreeBlock;
    }

    return NULL;
}


void* placeBlock (Block *blockPtr, size_t size)
{
    removeBlockFromFreeList(blockPtr);
    size_t remainingSpace = blockSize(blockPtr) - size;

    if (remainingSpace >= CHUNK_SIZE*2) {
        blockPtr->size = size | blockPtr->previousBlockInUse;

        Block *newFreeBlock = nextBlock(blockPtr);
        newFreeBlock->size = remainingSpace;
        newFreeBlock->previousBlockInUse = 1;
        *blockFooter(newFreeBlock) = remainingSpace;

        addBlockToFreeList(newFreeBlock);
    }
    else {
        nextBlock(blockPtr)->previousBlockInUse = 1;
    }

    return  blockPtr;
}


Block* extendDataMemorySegment (size_t size)
{
    struct shmid_ds shmStat;
    shmctl(dataMemorySegmentId, SHM_STAT, &shmStat);

    size_t additionalSpace = shmStat.shm_segsz;
    if (size >= shmStat.shm_segsz) {
        additionalSpace += alignSize(size, PAGE_SIZE);
    }
    size_t newDataSegmentSize = shmStat.shm_segsz + additionalSpace;

    int newDataMemorySegmentId = shmget(IPC_PRIVATE, newDataSegmentSize,
                                        IPC_CREAT | SHM_R | SHM_W);
    if (newDataMemorySegmentId == -1) perror("extendDataMemorySegment shmget");
    void *newDataMemorySegment = shmat(newDataMemorySegmentId, NULL, 0);

    memcpy(newDataMemorySegment, dataMemorySegment, shmStat.shm_segsz);

    shmdt(dataMemorySegment);
    shmctl(dataMemorySegmentId, IPC_RMID, NULL);

    dataMemorySegmentId = newDataMemorySegmentId;
    dataMemorySegment = newDataMemorySegment;

    Block *freeBlock = shmres(dataMemorySegment->stopBlock);
    freeBlock->size = additionalSpace | freeBlock->previousBlockInUse;
    *blockFooter(freeBlock) = additionalSpace;
    addBlockToFreeList(freeBlock);

    placeStopBlock(newDataSegmentSize);

    freeBlock = coalesceFreeBlocks(freeBlock);

    // Sende Reattach-Signal an Kind-Prozesse
    updateMemorySegment->dataMemorySegmentId = dataMemorySegmentId;
    killpg(updateProcessGroup, SIGUSR1);

    printf("Shared memory segment extended (Id %d, size %zu)\n",
           dataMemorySegmentId, newDataSegmentSize);

    return freeBlock;
}


void reattachDataMemorySegment ()
{
    if (dataMemorySegmentId != updateMemorySegment->dataMemorySegmentId) {
        shmdt(dataMemorySegment);
        dataMemorySegment = shmat(updateMemorySegment->dataMemorySegmentId, NULL, 0);
        dataMemorySegmentId = updateMemorySegment->dataMemorySegmentId;
    }
}


void shminit (size_t capacity)
{
    capacity = alignSize(((capacity <= CHUNK_SIZE) ? CHUNK_SIZE : capacity) +
            BLOCK_HEADER_SIZE, CHUNK_SIZE); // Genug Speicher für mindestens einen Block, CHUNK_SIZE-Einheiten
    size_t dataSegmentSize = alignSize(capacity + sizeof(DataMemorySegmentHeader) +
            CHUNK_SIZE, PAGE_SIZE); // + DataMemorySegmentHeader + StopBlock, PAGE_SIZE-Einheiten
    capacity = dataSegmentSize - sizeof(DataMemorySegmentHeader) - CHUNK_SIZE; // Nutzbarer Speicher

    // Erzeuge updateMemorySegment
    updateMemorySegmentId = shmget(IPC_PRIVATE, sizeof(UpdateMemorySegment),
                                   IPC_CREAT | SHM_R | SHM_W);
    if (updateMemorySegmentId == -1) perror("shminit shmget");
    updateMemorySegment = shmat(updateMemorySegmentId, NULL, 0);

    // Erzeuge dataMemorySegment
    dataMemorySegmentId = shmget(IPC_PRIVATE, dataSegmentSize, IPC_CREAT | SHM_R | SHM_W);
    if (dataMemorySegmentId == -1) perror("shminit shmget");
    dataMemorySegment = shmat(dataMemorySegmentId, NULL, 0);

    // Initialisiere updateMemorySegment
    updateMemorySegment->dataMemorySegmentId = dataMemorySegmentId;

    // Initialisiere dataMemorySegment
    dataMemorySegment->firstFreeBlock = NULL;

    Block *freeBlock = shmres((void*)sizeof(DataMemorySegmentHeader));
    freeBlock->size = capacity;
    *blockFooter(freeBlock) = capacity;
    freeBlock->previousBlockInUse = 1; // Hier gibt es Nichts (zum Zusammenfügen)
    addBlockToFreeList(freeBlock);

    placeStopBlock(dataSegmentSize);

    // Registriere Reattach-Signal für zukünftige Kind-Prozesse
    updateProcessGroup = getpgrp();
    signal(SIGUSR1, reattachDataMemorySegment);

    printf("Shared memory segment initialized (Id %d, size %zu)\n",
           dataMemorySegmentId, dataSegmentSize);
}


void shmcleanup ()
{
    shmdt(dataMemorySegment);
    shmctl(dataMemorySegmentId, IPC_RMID, NULL);

    shmdt(updateMemorySegment);
    shmctl(updateMemorySegmentId, IPC_RMID, NULL);

    printf("Shared memory segment removed (Id %d)\n", dataMemorySegmentId);
}


void* shmalloc (size_t size)
{
    size = alignSize(((size <= CHUNK_SIZE) ? CHUNK_SIZE : size) +
            BLOCK_HEADER_SIZE, CHUNK_SIZE);

    Block *fittingFreeBlock = findFittingFreeBlock(size);
    if (fittingFreeBlock == NULL) {
        fittingFreeBlock = extendDataMemorySegment(size);
    }

    Block *allocatedBlock = placeBlock(fittingFreeBlock, size);

    return shmdis((byte *) allocatedBlock + BLOCK_HEADER_SIZE);
}


void* shmrealloc (void* ptr, size_t size)
{
    size = alignSize(((size <= CHUNK_SIZE) ? CHUNK_SIZE : size), CHUNK_SIZE);

    Block *block = (Block*)((byte*)ptr - BLOCK_HEADER_SIZE);
    Block *blockNext = nextBlock(block);

    if (blockInUse(blockNext) ||
            (blockSize(block) + blockSize(blockNext)) < size) {
        void *newPtr = shmalloc(size);
        memcpy(shmres(newPtr), ptr, blockSize(block));
        shmfree(ptr);
        return newPtr;
    }

    blockNext = placeBlock(blockNext, size);
    block->size += blockSize(blockNext);
    *blockFooter(block) = blockSize(block);

    return ptr;
}


void shmfree (void* ptr)
{
    Block *block = (Block*)((byte*)ptr - BLOCK_HEADER_SIZE);
    nextBlock(block)->previousBlockInUse = 0;
    *blockFooter(block) = blockSize(block);

    addBlockToFreeList(block);
    coalesceFreeBlocks(block);
}


inline void* shmres (void* ptr)
{
    return (void*)((byte*)dataMemorySegment + (size_t)ptr);
}


inline void* shmdis (void* ptr)
{
    return (void*)((byte*)ptr - (byte*)dataMemorySegment);
}

