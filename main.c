#include "utils.h"
#include "command.h"
#include "storage.h"
#include "lock.h"
#include "newsletter.h"
#include "systemExec.h"
#include "network.h"

#include <signal.h>


void initAllModules ()
{
    initModulCommand();
    initModulStorage();
    initModulLock();
    initModulNewsletter();
    initModulSystemExec();
    initModulNetwork();
}


void freeAllModules ()
{
    freeModulNetwork();
    freeModulSystemExec();
    freeModulNewsletter();
    freeModulLock();
    freeModulStorage();
    freeModulCommand();
}


void freeResourcesAndExit ()
{
    freeAllModules();
    exit(EXIT_SUCCESS);
}


void fatalError (const char *message)
{
    perror(message);
    freeAllModules();
    exit(EXIT_FAILURE);
}


int main (int argc, char *argv[])
{
    initAllModules();

    if (fork() == 0) {
        runServerLoop("HTTP", HTTP_SERVER_PORT, clientHandlerHttp);
    }
    else {
        signal(SIGINT, freeResourcesAndExit);
        //signal(SIGTERM, freeResourcesAndExit);

        runServerLoop("Command", COMMAND_SERVER_PORT, clientHandlerCommand);
    }

    return 0;
}
