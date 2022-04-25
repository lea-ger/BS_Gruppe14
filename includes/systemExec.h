#ifndef SERVER_SYSTEMEXEC_H
#define SERVER_SYSTEMEXEC_H

#include "utils.h"
#include "command.h"

#include <stdio.h>


void eventCommandOperation (Command *cmd);

void initModulSystemExec ();
void freeModulSystemExec ();


#endif //SERVER_SYSTEMEXEC_H
