#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H


#include "utils.h"
#include "command.h"
#include "httpInterface.h"
#include "lock.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>


#define SOCKET int
#define RECV_BUFFER_SIZE PAGE_SIZE

#define COMMAND_SERVER_PORT 5678
#define HTTP_SERVER_PORT 5680

extern SOCKET processSocket;


void initModulNetwork (bool httpInterface);
void freeModulNetwork ();

void eventCommandQuit (Command *cmd);

void runServerLoop (const char* name, int port, void (*clientHandler)(SOCKET socket));
void clientHandlerCommand (SOCKET socket);
void clientHandlerHttp (SOCKET socket);

size_t receiveMessage (SOCKET socket, char* message);


#endif //SERVER_NETWORK_H
