#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H

/*
 * Netzwerk-Kommunikation
 *
 * Server-Loop zur Annahme eingehender TCP/IP Verbindungen und
 * Client-Handler zur Ein- und Ausgabe der Nutzdaten.
 *
 */

#include "utils.h"
#include "command.h"
#include "httpInterface.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>


#define SOCKET int
#define RECV_BUFFER_SIZE 4096

#define COMMAND_SERVER_PORT 5678
#define HTTP_SERVER_PORT 5680


void initModulNetwork ();
void freeModulNetwork ();

void eventCommandQuit (Command *cmd);

void runServerLoop (const char* name, int port, void (*clientHandler)(SOCKET socket));
void clientHandlerCommand (SOCKET socket);
void clientHandlerHttp (SOCKET socket);

int receiveMessage (SOCKET client, char* message);


#endif //SERVER_NETWORK_H
