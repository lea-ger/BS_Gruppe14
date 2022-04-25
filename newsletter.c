#include "newsletter.h"


void initModulNewsletter ()
{
    registerCommandEntry("SUB", 1, false, eventCommandSubscribe);
}


void freeModulNewsletter ()
{
}


void eventCommandSubscribe (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}
