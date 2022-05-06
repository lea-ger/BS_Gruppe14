#include <wait.h>
#include "newsletter.h"


// FIXME PLAYGROUND-STATUS!!!


static int observerPid = 0;
static RecordSubscriberMask subscriberId = 0;
static int subscriptionCounter = 0;

static int shmNewsletterSegmentId = 0;
static RecordSubscriberMask *subscriberRegistry = NULL;
static RecordSubscriberMask *subscribers = NULL;

static int msqNotifierId = 0;


void initModuleNewsletter ()
{
    registerCommandEntry("SUB", 1, false, eventCommandSubscribe);

    int segmentSize = sizeof(RecordSubscriberMask) * STORAGE_ENTRY_SIZE + 1;

    shmNewsletterSegmentId = shmget(IPC_PRIVATE, segmentSize, IPC_CREAT | SHM_R | SHM_W);
    if (shmNewsletterSegmentId == -1) {
        fatalError("initModuleNewsletter shmget");
    }
    printf("Newsletter shared memory segment created (Id %d).\n", shmNewsletterSegmentId);

    // Hängt das Shared-Memory-Segment in den lokalen Adressenraum ein
    // (Das Einhängen wird beim Erzeugen von Kind-Prozessen vererbt)
    subscriberRegistry = shmat(shmNewsletterSegmentId, NULL, 0);
    subscribers = &subscriberRegistry[1];
    memset(subscriberRegistry, 0, segmentSize);

    msqNotifierId = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
}


void freeModuleNewsletter ()
{
    msgctl(msqNotifierId, IPC_RMID, NULL);

    // Hängt das Shared-Memory-Segment aus dem lokalen Adressenraum aus
    shmdt(subscribers);
    // Löscht das Shared-Memory-Segment
    shmctl(shmNewsletterSegmentId, IPC_RMID, NULL);

    printf("Newsletter shared memory segment created (Id %d).\n", shmNewsletterSegmentId);
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


int subscribeStorageRecord (const char* key)
{
    //enterCriticalSection(READ_ACCESS);
    int recordIndex = findStorageRecord(key);
    if (recordIndex == -1) return 3; // key_nonexistent

    if (subscriberId == 0 && !takeSubscriberId()) {
        return 2; // subscribers_full
    }
    if (subscriberId & subscribers[recordIndex]) {
        return 1; // already_subscribed
    }

    subscribers[recordIndex] |= subscriberId;

    MsqBuffer msqBuffer = {.subscriberId=subscriberId,.newsletter={.notification=NL_NOTIFICATION_SUB}};
    if (msgsnd(msqNotifierId, &msqBuffer, sizeof(Newsletter), 0) < 0) {
        perror("subscribeStorageRecord msgsnd");
    }

    //leaveCriticalSection(READ_ACCESS);

    return 0; // subscribed
}


void notifyAllObservers (int notificationId, int recordIndex, const char* key, const char* value)
{
    if (shmNewsletterSegmentId == 0) return; // Modul nicht initialisiert
    
    MsqBuffer msqBuffer;
    msqBuffer.newsletter.notification = notificationId;
    strcpy(msqBuffer.newsletter.key, key);
    strcpy(msqBuffer.newsletter.value, value);

    for (int i = 0; i < NEWSLETTER_MAX_SUBS; i++) {
        msqBuffer.subscriberId = subscribers[recordIndex] & ((RecordSubscriberMask)1 << i);

        if (msqBuffer.subscriberId != 0) {
            if (notificationId == NL_NOTIFICATION_DEL) {
                subscribers[recordIndex] &= ~msqBuffer.subscriberId;

                // Wenn der Subscriber selbst einen seiner beobachteten Einträge löscht,
                // soll er keine DEL Notification bekommen, aber der Observer muss trotzdem
                // den subscriptionCounter herunterzählen. Wandelt deshalb die DEL- in eine UNSUB
                // Notification um.
                if (msqBuffer.subscriberId == subscriberId) {
                    msqBuffer.newsletter.notification = NL_NOTIFICATION_UNSUB;
                }
            } else if (msqBuffer.subscriberId == subscriberId) {
                continue;
            }

            if (msgsnd(msqNotifierId, &msqBuffer, sizeof(Newsletter), 0) < 0) {
                perror("notifyAllObservers msgsnd");
            }
        }
    }
}


bool takeSubscriberId ()
{
    // Wahr wenn alle Bits auf 1 stehen / keine Id mehr frei ist
    if (!~*subscriberRegistry) {
        return false;
    }
    // Setzte subscriberId auf das erste freie Bit in subscriberRegistry
    for (subscriberId = 1; subscriberId & *subscriberRegistry; subscriberId <<= 1);
    // Trägt das subscriberId-Bit in subscriberRegistry ein
    *subscriberRegistry |= subscriberId;

    signal(SIGCHLD, cleanupStorageObserver);
    observerPid = fork();
    if (observerPid == 0) {
        prctl(PR_SET_NAME, (unsigned long)"kvsvr(sub)");
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        signal(SIGTERM, releaseSubscriberId);
        runStorageObserver();

        exit(EXIT_SUCCESS);
    }
    return true;
}


void releaseSubscriberId ()
{
    // Wahr, wenn die Subscriber-Id nicht registriert ist
    if (!(*subscriberRegistry & subscriberId)) return;

    *subscriberRegistry &= ~subscriberId;

    if (subscriptionCounter > 0) {
        // Entfernt alle verbleibenden Subscriptions aus der Tabelle
        for (int i = 0; i < STORAGE_ENTRY_SIZE; i++) {
            subscribers[i] &= ~subscriberId;
        }
    }

    // Entferne mögliche nicht abgeholte Nachrichten aus der Warteschlange
    MsqBuffer msgBuffer;
    while (msgrcv(msqNotifierId, &msgBuffer, sizeof(MsqBuffer),
                  subscriberId, IPC_NOWAIT) > 0);
}


void runStorageObserver ()
{
    MsqBuffer msqBuffer;
    String *message = stringCreate("");
    Command *command = commandCreate();

    do {
        size_t response = msgrcv(msqNotifierId, &msqBuffer, sizeof(Newsletter),
                                 subscriberId, 0);
        if (response == -1) {
            if (errno != EINTR) {
                perror("runStorageObserver msgrcv");
            }
            return;
        }

        const char *commandName = "";
        switch (msqBuffer.newsletter.notification) {
            case NL_NOTIFICATION_SUB:
                subscriptionCounter++;
                break;
            case NL_NOTIFICATION_DEL:
                commandName = "DEL";
            case NL_NOTIFICATION_UNSUB:
                subscriptionCounter--;
                break;
            case NL_NOTIFICATION_PUT:
                commandName = "PUT";
        }

        if (*commandName != '\0') {
            stringCopy(command->name, commandName);
            stringCopy(command->key, msqBuffer.newsletter.key);
            stringCopy(command->value, msqBuffer.newsletter.value);
            responseRecordsFree(command->responseRecords);
            responseRecordsAdd(command->responseRecords,
                               command->key->cStr, command->value->cStr);

            commandFormatResponseMessage(command, message);

            response = send(processSocket, message->cStr, stringLength(message), 0);
            if (response == -1) {
                perror("runStorageObserver send");
                return;
            }
        }
    } while (subscriptionCounter > 0);

    stringFree(message);
    commandFree(command);
}


void cleanupStorageObserver ()
{
    int pid = wait(NULL);
    if (pid == observerPid) {
        observerPid = 0;
        subscriberId = 0;
    }
}

