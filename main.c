#include "utils.h"
#include "command.h"
#include "lock.h"
#include "storage.h"
//#include "newsletter.h"
#include "systemExec.h"
#include "network.h"


static int argSnapshotInterval = 0; // 0 = Snapshot-Timer deaktiviert
static bool argHttpInterface = true;
static bool argNewsletter = true;
static bool argSystemExec = true;


static void initAllModules ()
{
    initModuleCommand();
    initModuleLock();
    initModuleStorage(argSnapshotInterval);
    //if (argNewsletter) initModuleNewsletter();
    if (argSystemExec) initModuleSystemExec();
    initModuleNetwork(argHttpInterface);
}


static void freeAllModules ()
{
    freeModuleNetwork();
    if (argSystemExec) freeModuleSystemExec();
    //if (argNewsletter) freeModuleNewsletter();
    freeModuleStorage();
    freeModuleLock();
    freeModuleCommand();
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
