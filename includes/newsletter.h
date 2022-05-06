#ifndef SERVER_NEWSLETTER_H
#define SERVER_NEWSLETTER_H

#include "utils.h"
#include "command.h"
#include "storage.h"
#include "network.h"

#include <sys/msg.h>
#include <errno.h>


#define NEWSLETTER_MAX_SUBS sizeof(RecordSubscriberMask) * 8

#define NL_NOTIFICATION_SUB 0
#define NL_NOTIFICATION_UNSUB 1
#define NL_NOTIFICATION_PUT 2
#define NL_NOTIFICATION_DEL 3

typedef long RecordSubscriberMask;

typedef struct {
    int notification;
    char key[STORAGE_KEY_SIZE];
    char value[STORAGE_VALUE_SIZE];
} Newsletter;

typedef struct {
    RecordSubscriberMask subscriberId;
    Newsletter newsletter;
} MsqBuffer;


void eventCommandSubscribe (Command *cmd);

void initModuleNewsletter ();
void freeModuleNewsletter ();

void notifyAllObservers (int notificationId, int recordIndex, const char* key, const char* value);

int subscribeStorageRecord (const char* key);
bool takeSubscriberId ();

void runStorageObserver ();
void releaseSubscriberId ();
void cleanupStorageObserver ();


#endif //SERVER_NEWSLETTER_H
