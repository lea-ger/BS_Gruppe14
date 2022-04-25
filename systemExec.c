#include "systemExec.h"


void initModulSystemExec ()
{
    registerCommandEntry("OP", 2, false, eventCommandOperation);
}


void freeModulSystemExec ()
{

}


void eventCommandOperation (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "NOT_IMPLEMENTED_YET");
}