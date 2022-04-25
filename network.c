#include "network.h"


static const char *commandQuitName = "QUIT";
static SOCKET listener = 0;


void initModulNetwork ()
{
    registerCommandEntry(commandQuitName, 0, false, eventCommandQuit);
}


void freeModulNetwork ()
{
    if (listener != 0)
        close(listener);
}


void eventCommandQuit (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "GOODBYE");
}


int receiveMessage (SOCKET client, char *message)
{
    int size = recv(client, message, RECV_BUFFER_SIZE, 0);

    if (size == RECV_BUFFER_SIZE) {
        char termSign = message[RECV_BUFFER_SIZE-1];
        if (termSign != '\0' && termSign != '\n')
            return RECV_BUFFER_SIZE+1;
    }

    if (size < 0) {
        perror("recv");
    }
    else {
        message[size] = '\0';
    }
    return size;
}


/**
 * Starte eine Schleife mit der TCP/IP Verbindungsannahme an einem bestimmten Port.
 * Für jede eingehende Verbindung wird ein neuer Prozess erzeugt, der dann
 * den Client-Handler ausführt.
 *
 * @param name - Server-Name
 * @param port - Server-Port
 * @param clientHandler - Eintrittsfunktion für Clients
 */
void runServerLoop (const char* name, int port, void (*clientHandler)(SOCKET socket))
{
    struct sockaddr_in serverAddr;
    listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        fatalError("socket");
    }

    memset(&serverAddr, 0, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    {
        // Gibt den Port nach Schließung des Sockets sofort wieder frei,
        // anstatt ihn für die Dauer von 2*Maximum Segment Lifetime (30-120 Sekunden)
        // in den TIME-WAIT Status zu gehen, um auf mögliche verzögerte Pakete zu warten.
        int flag = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    }

    if (bind(listener,(struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fatalError("bind");
    }

    if (listen(listener, 5) == -1) {
        fatalError("listen");
    }

    printf("%s-Server listening on port %i\n", name, port);

    struct sockaddr_in clientAddr;
    unsigned int len = sizeof(clientAddr);
    SOCKET clientSocket;

    for (;;) {
        clientSocket = accept(listener, (struct sockaddr*)&clientAddr, &len);
        if (clientSocket < 0) {
            fatalError("accept");
        }

        if (fork() == 0) {
            signal(SIGINT, SIG_DFL);
            //signal(SIGTERM, SIG_DFL);
            close(listener);

            printf("%s-Client %d (%s) connected\n", name, getpid(), inet_ntoa(clientAddr.sin_addr));
            clientHandler(clientSocket);

            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
            printf("%s-Client %d (%s) disconnected\n", name, getpid(), inet_ntoa(clientAddr.sin_addr));

            exit(EXIT_SUCCESS);
        }
        else {
            close(clientSocket);
        }
    }
}


/**
 * Eintrittsfunktion für Command-Clients. Nimmt in einer Schleife Befehle entgegen,
 * verarbeitet Sie, und gibt dann die Ergebnisse zurück (persistente Verbindung).
 *
 * @param client - Socket-Handler
 */
void clientHandlerCommand (SOCKET client)
{
    String *buffer = stringCreateWithCapacity("", RECV_BUFFER_SIZE);
    Command *cmd = commandCreate();

    int size = 0;

    do {
        size = receiveMessage(client, buffer->cStr);
        if (size > RECV_BUFFER_SIZE) {
            const char *exceedMsg = "BUFFER_EXCEEDED\r\n";
            send(client, exceedMsg, strlen(exceedMsg), 0);
            continue;
        }
        else if (size <= 0) {
            break;
        }

        commandParseInputMessage(cmd, buffer);
        commandExecute(cmd);
        commandFormatResponseMessage(cmd, buffer);

        send(client, buffer->cStr, stringLength(buffer), 0);

    } while (!stringEquals(cmd->name, commandQuitName));

    commandFree(cmd);
    stringFree(buffer);
}


/**
 * Eintrittsfunktion für Http-Clients. Nimmt eine einzelne Anfrage entgegen,
 * verarbeitet Sie, gibt das Ergebnis zurück und beendet den Prozess.
 *
 * @param client - Client-Handler
 */
void clientHandlerHttp (SOCKET client) {
    String *buffer = stringCreateWithCapacity("", RECV_BUFFER_SIZE);
    HttpRequest *request = httpRequestCreate();
    HttpResponse *response = httpResponseCreate();

    int size = receiveMessage(client, buffer->cStr);
    if (size > RECV_BUFFER_SIZE) {
        httpResponseSetup(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                          0, NULL, 0);
        httpResponseFormateMessage(response, buffer);

        // (Missbrauch von String.capacity)
        send(client, buffer->cStr, buffer->capacity, 0);
    }
    else if (size > 0) {
        httpRequestParseMessage(request, buffer);
        printf("%s %s\n", request->method->cStr, request->url->cStr);
        httpRequestProcess(request, response);
        httpResponseFormateMessage(response, buffer);

        // (Missbrauch von String.capacity)
        send(client, buffer->cStr, buffer->capacity, 0);
    }

    httpResponseFree(response);
    httpRequestFree(request);
    stringFree(buffer);
}
