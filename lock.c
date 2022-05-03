#include "lock.h"


static int storageSemaphoreId = 0;
static struct sembuf enterStorage = {0, -1, SEM_UNDO};
static struct sembuf leaveStorage = {0, 1, SEM_UNDO};
static struct sembuf enterReaderCounter = {1, -1, SEM_UNDO};
static struct sembuf leaveReaderCounter = {1, 1, SEM_UNDO};

static int readerCounterId = 0;
static int* readerCounter = NULL;

static bool keyholder = false; // FIXME: Freigabe vor Beendigung des Clients... passiert schon... aber warum?


void initModulLock ()
{
    registerCommandEntry("BEG", 0, false, eventCommandBeginn);
    registerCommandEntry("END", 0, false, eventCommandEnd);

    readerCounterId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | SHM_R | SHM_W);
    readerCounter = shmat(readerCounterId, NULL, 0);
    *readerCounter = 0;

    unsigned short marker[2] = {1, 1};
    storageSemaphoreId = semget(IPC_PRIVATE, 2, IPC_CREAT | 0644);
    semctl(storageSemaphoreId, 1, SETALL, marker);

    printf("Synchronization mechanisms created (Semaphore-Id %d).\n", storageSemaphoreId);
}


void freeModulLock ()
{
    semctl(storageSemaphoreId, 0, IPC_RMID);

    shmdt(readerCounter);
    shmctl(readerCounterId, IPC_RMID, NULL);

    printf("Synchronization mechanisms deleted (Semaphore-Id %d).\n", storageSemaphoreId);
}


void eventCommandBeginn (Command *cmd)
{
    stringCopy(cmd->responseMessage,  lockStorage() ? "lock_successful" : "already_locked");
}


void eventCommandEnd (Command *cmd)
{
    stringCopy(cmd->responseMessage,  unlockStorage() ? "unlock_successful" : "not_locked");
}


/**
 * Multi-Reader/Single-Writer lock
 *
 * @param accessType
 * @return
 */
inline void enterCriticalSection (int accessType)
{
    if (keyholder) return;

    if (accessType == READ_ACCESS) {
        semop(storageSemaphoreId, &enterReaderCounter, 1);
        (*readerCounter)++;
        if (*readerCounter == 1) {
            semop(storageSemaphoreId, &enterStorage, 1);
        }
        semop(storageSemaphoreId, &leaveReaderCounter, 1);
    }
    else if (accessType == WRITE_ACCESS) {
        semop(storageSemaphoreId, &enterStorage, 1);
    }
}


inline void leaveCriticalSection (int accessType)
{
    if (keyholder) return;

    if (accessType == READ_ACCESS) {
        semop(storageSemaphoreId, &enterReaderCounter, 1);
        (*readerCounter)--;
        if (*readerCounter == 0) {
            semop(storageSemaphoreId, &leaveStorage, 1);
        }
        semop(storageSemaphoreId, &leaveReaderCounter, 1);
    }
    else if (accessType == WRITE_ACCESS) {
        semop(storageSemaphoreId, &leaveStorage, 1);
    }
}


bool lockStorage ()
{
    if (!keyholder) {
        semop(storageSemaphoreId, &enterStorage, 1);
        keyholder = true;
        return true;
    }
    return false;
}


bool unlockStorage ()
{
    if (keyholder) {
        keyholder = false;
        semop(storageSemaphoreId, &leaveStorage, 1);
        return true;
    }
    return false;
}

