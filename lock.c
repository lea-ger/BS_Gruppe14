#include "lock.h"


void initModulLock ()
{
    registerCommandEntry("BEG", 0, false, eventCommandBeginn);
    registerCommandEntry("END", 0, false, eventCommandEnd);
}


void freeModulLock ()
{
}


void eventCommandBeginn (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}


void eventCommandEnd (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}

