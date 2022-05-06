#include "lock.h"


/*
 * Synchronisations-Mechanismus
 *
 * Stellt mithilfe von Semaphoren einen Mechanismus für die Erhaltung der
 * und Datenkonsistenz bei gleichzeitigem Zugriff und einen exklusiven Modus
 * für das Storage bereit.
 *
 */


// SEM_UNDO macht Operationen eines Prozesses rückgängig, wenn er sich beendet.
// D.h. in diesem Fall, dass ein Client den Storage automatisch wieder freigibt,
// wenn er sich im exklusiven Modus beendet.
static int storageSemaphoreId = 0;
static struct sembuf enterStorage = {.sem_num=0, .sem_op=-1, .sem_flg=SEM_UNDO};
static struct sembuf leaveStorage = {.sem_num=0, .sem_op=1, .sem_flg=SEM_UNDO};
static struct sembuf enterReaderCounter = {.sem_num=1, .sem_op=-1, .sem_flg=SEM_UNDO};
static struct sembuf leaveReaderCounter = {.sem_num=1, .sem_op=1, .sem_flg=SEM_UNDO};

static int shmReaderCounterId = 0;
static int* readerCounter = NULL;

static bool exclusiveMode = false;


void initModuleLock ()
{
    registerCommandEntry("BEG", 0, false, eventCommandBeginn);
    registerCommandEntry("END", 0, false, eventCommandEnd);

    shmReaderCounterId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | SHM_R | SHM_W);
    if (shmReaderCounterId == -1) {
        fatalError("initModuleLock shmget");
    }
    readerCounter = shmat(shmReaderCounterId, NULL, 0);
    *readerCounter = 0;

    unsigned short marker[2] = {1, 1};
    storageSemaphoreId = semget(IPC_PRIVATE, 2, IPC_CREAT | 0644);
    if (shmReaderCounterId == -1) {
        fatalError("initModuleLock semget");
    }
    semctl(storageSemaphoreId, 1, SETALL, marker);

    printf("Synchronization mechanisms created (Semaphore-Id %d, Sh.Mem.-Id %d).\n",
           storageSemaphoreId, shmReaderCounterId);
}


void freeModuleLock ()
{
    semctl(storageSemaphoreId, 0, IPC_RMID);

    shmdt(readerCounter);
    shmctl(shmReaderCounterId, IPC_RMID, NULL);

    printf("Synchronization mechanisms deleted (Semaphore-Id %d, Sh.Mem.-Id %d).\n",
           storageSemaphoreId, shmReaderCounterId);
}


void eventCommandBeginn (Command *cmd)
{
    stringCopy(cmd->responseMessage, enterExclusiveMode() ? "locked" : "already_locked");
}


void eventCommandEnd (Command *cmd)
{
    stringCopy(cmd->responseMessage, leaveExclusiveMode() ? "unlocked" : "not_locked");
}


/**
 * Multi-Reader/Single-Writer Lock
 *
 * In dieser Lösung des Leser/Schreiber-Problems werden Leser stark bevorzugt
 * und haben uneingeschränkten gleichzeitigen Zugriff. Schreibzugriffe sind nur
 * möglich wenn es gerade keine Leser gibt. Der Nachteil ist, das die Gefahr
 * besteht dass Schreiber verhungern.
 *
 * @param accessType - Datenzugriffs-Art
 */
inline void enterCriticalSection (int accessType)
{
    if (exclusiveMode) return;

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
    if (exclusiveMode) return;

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


/**
 * Realisiert den exklusiven Zugriff mit einer Down-Operation des
 * Semaphore welcher den kritischen Bereich schützt. Wenn ein Client
 * in den exklusiven Modus geht während dieser bereits genutzt wird,
 * wird er selbst blockiert. (Schlecht für menschliche Interaktion,
 * akzeptabel für IPC?)
 *
 */
bool enterExclusiveMode ()
{
    if (!exclusiveMode) {
        semop(storageSemaphoreId, &enterStorage, 1);
        exclusiveMode = true;
        return true;
    }
    return false;
}


bool leaveExclusiveMode ()
{
    if (exclusiveMode) {
        exclusiveMode = false;
        semop(storageSemaphoreId, &leaveStorage, 1);
        return true;
    }
    return false;
}

