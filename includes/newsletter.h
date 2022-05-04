#ifndef SERVER_NEWSLETTER_H
#define SERVER_NEWSLETTER_H

#include "utils.h"
#include "command.h"
#include "storage.h"
#include "network.h"

#include <sys/msg.h>
#include <errno.h>


#define NEWSLETTER_MAX_SUBS sizeof(RecordSubscriberMask) * 8

#define NL_NOTIFY_PUT 0
#define NL_NOTIFY_DEL 1

typedef long RecordSubscriberMask;

typedef struct
{
    long mtype;
    char mtext[64 + 256 + 7]; // FIXME
} MsqBuffer;


void eventCommandSubscribe (Command *cmd);

void initModulNewsletter ();
void freeModulNewsletter ();

int subscribeStorageRecord (const char* recordKey);
int unsubscribeStorageRecord (const char* recordKey);
int subscribeStorageRecordWithIndex (int recordIndex);
int unsubscribeStorageRecordWithIndex (int recordIndex);

void notifyNewsletter (int cmdId, int index, const char* key, const char* value);

void runSocketForwarder ();
bool takeSubscriberId ();
void releaseSubscriberId ();


#endif //SERVER_NEWSLETTER_H
