#include <sys/wait.h>
#include "systemExec.h"


/*
 * System-Program Executor
 *
 * Nimmt Storage-Einträge als Ein-/Ausgabe für
 * eine Systemanwendung (z.B. "date" oder der Taschenrechner "bc").
 *
 */


void initModuleSystemExec ()
{
    registerCommandEntry("OP", 2, false, eventCommandOperation);
}


void freeModuleSystemExec ()
{
}


void eventCommandOperation (Command *cmd)
{
    String *inputBuffer = stringCreate("");
    String *outputBuffer = stringCreate("");

    getStorageRecord(cmd->key->cStr, inputBuffer);

    bool success = executeOperation(cmd->value->cStr,
                                    inputBuffer->cStr, outputBuffer);
    stringCopy(cmd->responseMessage, (success) ? "op_successful" : "op_failed");

    if (!stringIsEmpty(outputBuffer)) {
        putStorageRecord(cmd->key->cStr, outputBuffer->cStr);
    }

    stringFree(outputBuffer);
    stringFree(inputBuffer);
}


bool executeOperation (const char *op, const char* input, String *output)
{
    // Lazier version
#if 0
    String *pipedCommand = stringCreateWithFormat("echo \"%s\" | %s", input, op);

    FILE *pFile = popen(pipedCommand->cStr, "r");
    fgets(output->cStr, PIPE_BUFFER_SIZE, pFile);
    pclose(pFile);

    stringTrimSpaces(output);
    stringFree(pipedCommand);
#endif

    stringReserve(output, PIPE_BUFFER_SIZE);

    // Erstelle 2 neue Pipes
    int inputPipe[2], outputPipe[2];
    pipe(inputPipe);
    pipe(outputPipe);

    // Erzeugt einen neuen Prozess, verbindet die Standard Ein-/Ausgabe mit den Pipes
    // und ersetzt den Programmcode des Prozesses / führt ein anderes Programm aus.
    int execPid = fork();
    if (execPid == 0) {
        // Schliesst die Pipe-Seiten die vom Eltern-Prozess benutzt werden
        close(inputPipe[1]);
        close(outputPipe[0]);

        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        //dup2(outputPipe[1], STDERR_FILENO);

        execl("/bin/sh", "sh", "-c", op, NULL);

        exit(EXIT_FAILURE);
    }

    // Schliesst die Pipe-Seiten die vom Kind-Prozess benutzt werden
    close(inputPipe[0]);
    close(outputPipe[1]);

    write(inputPipe[1], input, strlen(input));
    write(inputPipe[1], "\n", 1);
    close(inputPipe[1]);

    for (int i = 0; i < PIPE_BUFFER_SIZE; i++) {
        read(outputPipe[0], &output->cStr[i], 1);
        if (output->cStr[i] == '\n') {
            output->cStr[i] = '\0';
            break;
        }
    }
    close(outputPipe[0]);

    // Warte auf den exit-Status des Programms
    int status;
    waitpid(execPid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status) == 0;
    }

    return false;
}
