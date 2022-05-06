#include "network.h"


/*
 * Netzwerk-Kommunikation
 *
 * Server-Loop zur Annahme eingehender TCP/IP Verbindungen und
 * Client-Handler zur Ein- und Ausgabe der Nutzdaten.
 *
 */


static const char *commandQuitName = "QUIT";
SOCKET processSocket = 0;


void initModuleNetwork (bool httpInterface)
{
    registerCommandEntry(commandQuitName, 0, false, eventCommandQuit);

    if (httpInterface && fork() == 0) {
        prctl(PR_SET_NAME, (unsigned long)"kvsvr(http)");
        runServerLoop("Http", HTTP_SERVER_PORT, clientHandlerHttp);

        exit(EXIT_SUCCESS);
    }
}


void freeModuleNetwork ()
{
    if (processSocket != 0)
        close(processSocket);
}


void eventCommandQuit (Command *cmd)
{
    stringCopy(cmd->responseMessage,  "goodbye");
}


/**
 * Empfängt eine einzelne Text-Nachrichten von einer Socket-Verbindung.
 *
 * Wenn die Puffergröße RECV_BUFFER_SIZE erreicht wurde aber die
 * Nachricht weder mit einer String-Terminierung noch einem Zeilenumbruch
 * endet wird davon ausgegangen das der Puffer überschritten wurde.
 * Dann ist der Rückgabewert größer als RECV_BUFFER_SIZE. Bei vorhandener Termini
 *
 * @param socket - Verbindungs-Deskriptor
 * @param message - Eingangsnachricht
 */
size_t receiveMessage (SOCKET socket, String *message)
{
    stringReserve(message, RECV_BUFFER_SIZE);

    size_t size = recv(socket, message->cStr, RECV_BUFFER_SIZE, 0);
    if (size < 0) {
        perror("receiveMessage recv");
    }
    if (size == RECV_BUFFER_SIZE) {
        char termSign = message->cStr[size-1];
        if (termSign != '\0' && termSign != '\n') {
            message->length = size-1;
            message->cStr[size] = '\0';
            return RECV_BUFFER_SIZE+1;
        }
    }

    message->length = size;
    message->cStr[size] = '\0';

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

    // Erzeugt ein TCP/IP(v4) Socket ("Rendezvous-Descriptor")
    processSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (processSocket < 0) {
        fatalError("runServerLoop socket");
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
        setsockopt(processSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    }

    // Bindet den Socket an eine Adresse
    if (bind(processSocket,(struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fatalError("runServerLoop bind");
    }

    // Eingehende Verbindungen in einer Warteschlange aufnehmen
    if (listen(processSocket, 5) == -1) {
        fatalError("runServerLoop listen");
    }

    printf("%s-Server listening on port %i\n", name, port);

    struct sockaddr_in clientAddr;
    unsigned int len = sizeof(clientAddr);
    SOCKET clientSocket;

    for (;;) {
        // Eingehende Verbindung aus der Warteschlange akzeptieren
        clientSocket = accept(processSocket, (struct sockaddr*)&clientAddr, &len);
        if (clientSocket < 0) {
            fatalError("runServerLoop accept");
        }

        // Neuer Client-Handler Prozess
        if (fork() == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            close(processSocket);

            processSocket = clientSocket;

            printf("%s-Client %d (%s) connected\n", name, getpid(), inet_ntoa(clientAddr.sin_addr));
            clientHandler(clientSocket);
            close(clientSocket);
            printf("%s-Client %d (%s) disconnected\n", name, getpid(), inet_ntoa(clientAddr.sin_addr));

            exit(EXIT_SUCCESS);
        }
        // Server Prozess
        else {
            close(clientSocket);
        }
    }
}


/**
 * Eintrittsfunktion für Command-Clients. Nimmt in einer Schleife Befehle entgegen,
 * verarbeitet Sie, und gibt dann die Ergebnisse zurück (persistente Verbindung).
 *
 * @param socket - Verbindungs-Descriptor
 */
void clientHandlerCommand (SOCKET socket)
{
    prctl(PR_SET_NAME, (unsigned long)"kvsvr(cmd-cli)");

    String *buffer = stringCreateWithCapacity("", RECV_BUFFER_SIZE);
    Command *cmd = commandCreate();

    do {
        size_t size = receiveMessage(socket, buffer);
        if (size > RECV_BUFFER_SIZE) {
            const char *exceedMsg = "BUFFER_EXCEEDED\r\n";
            send(socket, exceedMsg, strlen(exceedMsg), 0);
            continue;
        }
        else if (size <= 0) {
            break;
        }

        commandParseInputMessage(cmd, buffer);
        printf("Cmd-%d: %s %s %s\n", getpid(),
               cmd->name->cStr, cmd->key->cStr, cmd->value->cStr);
        commandExecute(cmd);
        commandFormatResponseMessage(cmd, buffer);

        send(socket, buffer->cStr, stringLength(buffer), 0);

    } while (!stringEquals(cmd->name, commandQuitName));

    commandFree(cmd);
    stringFree(buffer);
}


/**
 * Eintrittsfunktion für Http-Clients. Nimmt eine einzelne Anfrage entgegen,
 * verarbeitet Sie, gibt das Ergebnis zurück und beendet den Prozess.
 *
 * @param socket - Verbindungs-Descriptor
 */
void clientHandlerHttp (SOCKET socket) {
    prctl(PR_SET_NAME, (unsigned long)"kvsvr(http-cli)");

    String *buffer = stringCreateWithCapacity("", RECV_BUFFER_SIZE);
    HttpRequest *request = httpRequestCreate();
    HttpResponse *response = httpResponseCreate();

    size_t size = receiveMessage(socket, buffer);
    if (size > RECV_BUFFER_SIZE) {
        response->statusCode = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        httpResponseFormateMessage(response, buffer, &size);

        send(socket, buffer->cStr, size, 0);
    }
    else if (size > 0) {
        httpRequestParseMessage(request, buffer);
        printf("Http-%d: %s %s %s\n", getpid(),
               request->method->cStr, request->url->cStr, request->payload->cStr);
        httpRequestProcess(request, response);
        httpResponseFormateMessage(response, buffer, &size);

        send(socket, buffer->cStr, size, 0);
    }

    httpResponseFree(response);
    httpRequestFree(request);
    stringFree(buffer);
}
