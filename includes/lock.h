#ifndef SERVER_LOCK_H
#define SERVER_LOCK_H

#include "utils.h"
#include "command.h"


void eventCommandBeginn (Command *cmd);
void eventCommandEnd (Command *cmd);

void initModulLock ();
void freeModulLock ();


#endif //SERVER_LOCK_H
