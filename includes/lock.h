#ifndef SERVER_LOCK_H
#define SERVER_LOCK_H

#include "utils.h"
#include "command.h"

#include <time.h>
#include <sys/sem.h>
#include <sys/shm.h>


#define READ_ACCESS 0
#define WRITE_ACCESS 1


void eventCommandBeginn (Command *cmd);
void eventCommandEnd (Command *cmd);

void initModuleLock ();
void freeModuleLock ();

void enterCriticalSection (int accessType);
void leaveCriticalSection (int accessType);

bool enterExclusiveMode ();
bool leaveExclusiveMode ();


#endif //SERVER_LOCK_H
