#ifndef SERVER_SYSTEMEXEC_H
#define SERVER_SYSTEMEXEC_H

#include "utils.h"
#include "command.h"
#include "storage.h"

#include <stdio.h>


#define PIPE_BUFFER_SIZE STORAGE_VALUE_SIZE


void eventCommandOperation (Command *cmd);

void initModuleSystemExec ();
void freeModuleSystemExec ();

bool executeOperation (const char *op, const char *input, String *output);


#endif //SERVER_SYSTEMEXEC_H
