#include <sys/wait.h>
#include "systemExec.h"


// FIXME PLAYGROUND-STATUS!!!


void initModulSystemExec ()
{
    registerCommandEntry("OP", 2, false, eventCommandOperation);
}


void freeModulSystemExec ()
{
}


void eventCommandOperation (Command *cmd)
{
    const char *key = cmd->key->cStr;
    const char *operation = cmd->value->cStr;

    String *inputBuffer = stringCreate("");
    String *outputBuffer = stringCreate("");

    getStorageRecord(key, inputBuffer);

    bool success = executeOperation(operation, inputBuffer->cStr, outputBuffer);
    stringCopy(cmd->responseMessage, (success) ? "op_successful" : "op_failed");

    if (!stringIsEmpty(outputBuffer)) {
        putStorageRecord(key, outputBuffer->cStr);
    }
}


// FIXME Syntaxerror?!
// put calc 5+5
// op calc bc
bool executeOperation (const char *op, const char* input, String *output)
{
    stringReserve(output, PIPE_BUFFER_SIZE);

    int inputPipe[2], outputPipe[2];

    pipe(inputPipe);
    pipe(outputPipe);

    signal(SIGCHLD, SIG_DFL);
    int execPid = fork();

    if (execPid == 0) {
        //close(inputPipe[1]);
        //close(outputPipe[0]);

        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        //dup2(outputPipe[1], STDERR_FILENO);

        //prctl(PR_SET_PDEATHSIG, SIGTERM);

        execl("/bin/sh", op, "-c", op, NULL);

        exit(EXIT_FAILURE);
    }

    close(inputPipe[0]);
    close(outputPipe[1]);

    //FILE* fhWrite = fdopen(inputPipe[1], "w");
    //FILE* fhRead = fdopen(outputPipe[0], "r");

    //fwrite(input, strlen(input), 1, fhWrite);
    //fclose(fhWrite);

    //fgets(output->cStr, PIPE_BUFFER_SIZE, fhRead);
    //fclose(fhRead);

    stringTrimSpaces(output);

    write(inputPipe[1], input, strlen(input));
    read(outputPipe[0], output->cStr, PIPE_BUFFER_SIZE);

    int status;
    waitpid(execPid, &status, 0);

    signal(SIGCHLD, SIG_IGN);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status) == 0;
    }

    return false;
}
