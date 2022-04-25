#ifndef SERVER_STORAGE_H
#define SERVER_STORAGE_H

#include "utils.h"
#include "command.h"
#include "lock.h"

#include <stdio.h>


void eventCommandGet (Command *cmd);
void eventCommandPut (Command *cmd);
void eventCommandDel (Command *cmd);

void initModulStorage ();
void freeModulStorage ();


#endif //SERVER_STORAGE_H
