#include "utils.h"
#include "command.h"
#include "lock.h"
#include "storage.h"
#include "newsletter.h"
#include "systemExec.h"
#include "network.h"


void initAllModules ()
{
    initModulCommand();
    initModulLock();
    initModulStorage(false);
    initModulNewsletter();
    initModulSystemExec();
    initModulNetwork(true);
}


void freeAllModules ()
{
    freeModulNetwork();
    freeModulSystemExec();
    freeModulNewsletter();
    freeModulStorage();
    freeModulLock();
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
    // Verwerfe den exit-Status der Kind-Prozesse, um Zombie-Prozesse zu verhindern
    signal(SIGCHLD, SIG_IGN);

    initAllModules();

    signal(SIGINT, freeResourcesAndExit); // Beenden mit ctrl+c
    signal(SIGTERM, freeResourcesAndExit); // Beenden mit kill #pid

    prctl(PR_SET_NAME, (unsigned long)"kvsvr(cmd)");
    runServerLoop("Command", COMMAND_SERVER_PORT, clientHandlerCommand);

    return EXIT_SUCCESS;
}
