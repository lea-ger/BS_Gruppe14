#include "utils.h"
#include "command.h"
#include "storage.h"
#include "lock.h"
#include "newsletter.h"
#include "systemExec.h"
#include "network.h"

#include <signal.h>
#include <sys/prctl.h>


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
    prctl(PR_SET_NAME, (unsigned long)"kvsvr(cmd)");

    initAllModules();

    // Verwerfe den exit-Status der Kind-Prozesse, um Zombie-Prozesse zu verhindern
    signal(SIGCHLD, SIG_IGN);

    if (fork() == 0) {
        prctl(PR_SET_NAME, (unsigned long)"kvsvr(http)");

        runServerLoop("HTTP", HTTP_SERVER_PORT, clientHandlerHttp);
    }
    else {
        signal(SIGINT, freeResourcesAndExit); // Beenden mit ctrl+c
        signal(SIGTERM, freeResourcesAndExit); // Beenden mit kill #pid

        runServerLoop("Command", COMMAND_SERVER_PORT, clientHandlerCommand);
    }

    return 0;
}
