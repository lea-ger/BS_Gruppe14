#include "newsletter.h"


// FIXME PLAYGROUND-STATUS!!!


int socketForwarderPid = 0;
RecordSubscriberMask subscriberId = 0;
int subscriptionCounter = 0;

int shmStorageSegmentId = 0;
RecordSubscriberMask *usedSubscriberIds = NULL;
RecordSubscriberMask *subscribers = NULL;

int msqId = 0;


void initModulNewsletter ()
{
    registerCommandEntry("SUB", 1, false, eventCommandSubscribe);

    int segmentSize = sizeof(RecordSubscriberMask) * STORAGE_ENTRY_SIZE + 1;

    shmStorageSegmentId = shmget(IPC_PRIVATE, segmentSize, IPC_CREAT | SHM_R | SHM_W);
    if (shmStorageSegmentId == -1) {
        fatalError("shmget");
    }
    printf("Newsletter shared memory segment created (Id %d).\n", shmStorageSegmentId);

    // Hängt das Shared-Memory-Segment in den lokalen Adressenraum ein
    // (Das Einhängen wird beim Erzeugen von Kind-Prozessen vererbt)
    usedSubscriberIds = shmat(shmStorageSegmentId, NULL, 0);
    subscribers = &usedSubscriberIds[1];
    memset(usedSubscriberIds, 0, segmentSize);

    msqId = msgget(42, IPC_CREAT | 0666);
}


void freeModulNewsletter ()
{
    msgctl(msqId, IPC_RMID, NULL);

    // Hängt das Shared-Memory-Segment aus dem lokalen Adressenraum aus
    shmdt(subscribers);
    // Löscht das Shared-Memory-Segment
    shmctl(shmStorageSegmentId, IPC_RMID, NULL);

    printf("Newsletter shared memory segment created (Id %d).\n", shmStorageSegmentId);
}


void eventCommandSubscribe (Command *cmd)
{
    const char *message = "subscribed";
    int response = subscribeStorageRecord(cmd->key->cStr);
    switch (response) {
        case 1: message = "already_subscribed"; break;
        case 2: message = "subscribers_full"; break;
        case 3: message = "key_nonexistent"; break;
        default: break;
    }
    stringCopy(cmd->responseMessage,  message);
}


int subscribeStorageRecord (const char* recordKey)
{
    enterCriticalSection(READ_ACCESS);
    int index = findStorageRecord(recordKey);
    if (index == -1) return 3; // key_nonexistent
    int response = subscribeStorageRecordWithIndex(index);
    leaveCriticalSection(READ_ACCESS);

    return response;
}


int unsubscribeStorageRecord (const char* recordKey)
{
    enterCriticalSection(READ_ACCESS);
    int index = findStorageRecord(recordKey);
    if (index == -1) return 3; // key_nonexistent
    int response = unsubscribeStorageRecordWithIndex(index);
    leaveCriticalSection(READ_ACCESS);

    return response;
}


int subscribeStorageRecordWithIndex (int recordIndex)
{
    if (subscriptionCounter == 0 && !takeSubscriberId()) {
        return 2; // subscribers_full
    }

    RecordSubscriberMask *subMask = &subscribers[recordIndex];

    if (subscriberId & *subMask) {
        return 1; // already_subscribed
    }

    *subMask |= subscriberId;
    subscriptionCounter++;

    return 0; // subscribed
}


int unsubscribeStorageRecordWithIndex (int recordIndex)
{
    subscribers[recordIndex] &= ~subscriberId;
    subscriptionCounter--;
    if (subscriptionCounter == 0) {
        kill(socketForwarderPid, SIGKILL);
        socketForwarderPid = 0;
    }
}


void notifyNewsletter (int cmdId, int index, const char* key, const char* value)
{
    if (shmStorageSegmentId == 0) return;

    const char *cmdName = (cmdId == NL_NOTIFY_PUT) ? "PUT" : "DEL";
    RecordSubscriberMask *recordSubMask = &subscribers[index];

    // FIXME format nicht hier
    String *message = stringCreateWithFormat("%s:%s:%s\n",
                                         cmdName, key, value);
    MsqBuffer msqBuffer;
    strcpy(msqBuffer.mtext, message->cStr);
    stringFree(message);

    for (int i = 0; i < NEWSLETTER_MAX_SUBS; i++) {
        msqBuffer.mtype = (*recordSubMask)>>i & 1;

        if (msqBuffer.mtype && !(*recordSubMask & subscriberId)) {
            if (msgsnd(msqId, &msqBuffer, sizeof(MsqBuffer), 0)) {
                perror("msgsnd");
            }
            if (cmdId == NL_NOTIFY_DEL) {
                unsubscribeStorageRecordWithIndex(i);
            }
        }
    }
}


bool takeSubscriberId ()
{
    // Wahr wenn alle Bits auf 1 stehen / kein freier Platz vorhanden ist
    if (!~*usedSubscriberIds) {
        return false;
    }
    // Setzte subscriberId auf das erste freie Bit in usedSubscriberIds
    for (subscriberId = 1; subscriberId & *usedSubscriberIds; subscriberId <<= 1);
    // Trägt das subscriberId-Bit in usedSubscriberIds ein
    *usedSubscriberIds |= subscriberId;

    socketForwarderPid = fork();
    if (socketForwarderPid == 0) {
        prctl(PR_SET_NAME, (unsigned long)"kvsvr(sub)");
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGTERM, releaseSubscriberId);
        runSocketForwarder();

        exit(EXIT_SUCCESS);
    }
    return true;
}


void releaseSubscriberId ()
{
    if (subscriptionCounter > 0) {
        for (int i = 0; i < STORAGE_ENTRY_SIZE; i++) {
            subscribers[i] &= ~subscriberId;
        }
    }
    subscriptionCounter = 0;
    *usedSubscriberIds &= ~subscriberId;
    subscriberId = 0;
}


void runSocketForwarder ()
{
    String *message = stringCreate("");
    MsqBuffer msgBuffer;

    for (;;) {
        int v = msgrcv(msqId, &msgBuffer, sizeof(MsqBuffer),
                       subscriberId, 0);
        if (v == -1) {
            perror("runSocketForwarder msgrcv");
            return;
        }
        else {
            printf("send");
            send(processSocket, msgBuffer.mtext, strlen(msgBuffer.mtext), 0);
        }
    }

    // stringFree(message);
}

