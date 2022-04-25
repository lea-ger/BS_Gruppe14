#include "httpInterface.h"


/**
 * Erzeugt ein neues Objekt welches eine Http-Anfrage repräsentiert
 *
 */
HttpRequest* httpRequestCreate ()
{
    HttpRequest *request = malloc(sizeof(HttpRequest));

    request->method = stringCreate("");
    request->url = stringCreate("");
    request->payload = stringCreate("");

    return request;
}


/**
 * Gibt den Heap-Speicher des Zielobjekts wieder frei.
 *
 * @param request - Zielobjekt
 */
void httpRequestFree (HttpRequest *request)
{
    stringFree(request->method);
    stringFree(request->url);
    stringFree(request->payload);

    free(request);
}


/**
 * Erzeugt ein neues Objekt welches eine Http-Antwort repräsentiert
 *
 */
HttpResponse* httpResponseCreate ()
{
    HttpResponse *response = malloc(sizeof(HttpResponse));

    response->payload = stringCreate("");
    response->attributes = arrayCreate();

    return response;
}


/**
 * Gibt den Heap-Speicher des Zielobjekts wieder frei.
 *
 * @param response - Zielobjekt
 */
void httpResponseFree (HttpResponse *response)
{
    httpResponseAttributesFree(response);

    arrayFree(response->attributes);
    stringFree(response->payload);

    free(response);
}


/**
 * Setzt die Attribute eines Http-Response-Objekts.
 *
 * @param response - Zielobjekt
 * @param statusCode - HTTP Status
 * @param payloadSize - Größe des Anhangs
 * @param payload - Datenanhang
 * @param additionalAttributes - Anzahl der zusätzlichen HTTP Attribute
 * @param ... - HTTP Attribute
 */
void httpResponseSetup (HttpResponse *response, int statusCode,
                        size_t payloadSize, const char *payload, int additionalAttributes, ...)
{
    response->statusCode = statusCode;
    response->payloadSize = payloadSize;

    if (payload != NULL) {
        stringReserve(response->payload, payloadSize);
        memcpy(response->payload->cStr, payload, payloadSize);
    }

    va_list vaList;
    va_start(vaList, additionalAttributes);
    for (int i = 0; i < additionalAttributes; i++) {
        char *attribut = va_arg(vaList, char*);
        arrayPushItem(response->attributes, stringCreate(attribut));
    }
    va_end(vaList);
}


/**
 * Gibt den Heap-Speicher aller zusätzlicher Attribute (String Objekte) frei.
 *
 * @param response - Zielobjekt
 */
void httpResponseAttributesFree (HttpResponse *response)
{
    for (int i = 0; i < response->attributes->size; i++) {
        String *att = response->attributes->cArr[i];
        stringFree(att);
    }
    arrayClear(response->attributes);
}


/**
 * Interpretiert eine Http-Anfrage und setzt die entsprechenden
 * Attribute in das Http-Request-Objekt ein.
 *
 * @param request - Zielobjekt
 * @param requestMessage - Http-Anfrage
 */
void httpRequestParseMessage (HttpRequest *request, String *requestMessage)
{
    char* payload = strstr(requestMessage->cStr, "\r\n\r\n");
    char* method = strtok(requestMessage->cStr, " \t");
    char* url    = strtok(NULL, " \t");

    stringToUpper(stringCopy(request->method, (method != NULL) ? method : ""));
    stringCopy(request->url, (url != NULL) ? url : "");
    stringCopy(request->payload, (payload != NULL) ? &payload[4] : "");
}


/**
 * Verarbeitet eine Http-Anfrage und speichert die Ergebnisse in dem
 * Http-Response-Objekt. Leitet die Anfrage bei Aufruf der REST-API-URL an den
 * Befehlsverteiler weiter um auf die Datenbank zuzugreifen.
 *
 * @param request - Http-Anfrage
 * @param response - Http-Antwort
 */
void httpRequestProcess (HttpRequest *request, HttpResponse *response)
{
    // Zugriff auf die Datenbank
    // -------------------------
    if (strncmp(request->url->cStr, storageUrl, strlen(storageUrl)) == 0) {
        Command *cmd = commandCreate();

        stringCopy(cmd->name, request->method->cStr);
        stringCopy(cmd->key, request->url->cStr);
        stringCopy(cmd->value, request->payload->cStr);

        stringCut(cmd->key,strlen(storageUrl),
                  stringLength(cmd->key));

        if (stringEquals(cmd->name, "DELETE")) {
            stringCopy(cmd->name, "DEL");
        }
        if (!stringEquals(cmd->name, "GET") &&
                !stringEquals(cmd->name, "PUT") &&
                !stringEquals(cmd->name, "DEL")) {
            httpResponseSetup(response, HTTP_STATUS_METHOD_NOT_ALLOWED,
                              0, NULL, 1, "Allow: GET, PUT, DELETE");
            return;
        }

        commandExecute(cmd);
        commandToJson(cmd, response->payload);

        commandFree(cmd);

        httpResponseSetup(response, HTTP_STATUS_OK,
                          stringLength(response->payload), NULL,
                          1, "Content-Type: application/json");
        return;
    }
    // Zugriff auf das Webfile-Verzeichnis
    // -----------------------------------
    if (!stringEquals(request->method, "GET")) {
        httpResponseSetup(response, HTTP_STATUS_METHOD_NOT_ALLOWED,
                          0, NULL, 1, "Allow: GET");
        return;
    }

    httpResponseLoadWebfile(response, request->url->cStr);
}


/**
 * Formatiert eine Antwortnachricht aus dem Http-Response-Objekt.
 *
 * @param response - Zielobjekt
 * @param responseMessage - Antwortnachricht
 */
void httpResponseFormateMessage (HttpResponse *response, String *responseMessage)
{
    const char* statusName = getHttpStatusName(response->statusCode);

    stringCopyFormat(responseMessage, "\r\nHTTP/1.0 %d %s\r\n",
                     response->statusCode, statusName);

    if (response->payloadSize == 0) {
        stringCopyFormat(response->payload, "<html>\r\n"
                        "<head><title>%d %s</title></head>\r\n"
                        "<body>\r\n"
                        "<center><h1>%d %s</h1></center>\r\n"
                        "</body>\r\n"
                        "</html>\r\n",
                        response->statusCode, statusName,
                        response->statusCode, statusName);
        response->payloadSize = stringLength(response->payload);
    }

    stringAppendFormat(responseMessage, "Content-Length: %d\r\n", response->payloadSize);

    for (int i = 0; i < response->attributes->size; i++) {
        String *attribut = response->attributes->cArr[i];
        stringAppend(responseMessage, attribut->cStr);
        stringAppend(responseMessage, "\r\n");
    }
    stringAppend(responseMessage, "\r\n");

    stringReserve(responseMessage, stringLength(responseMessage) + response->payloadSize);
    memcpy(&responseMessage->cStr[stringLength(responseMessage)],
           response->payload->cStr, response->payloadSize);
}


/**
 * Läd eine Datei zum anhängen an die Http-Antwort. Verändert dabei
 * den Status und andere Attribute des Http-Response-Objekts.
 *
 * @param response - Zielobjekt
 * @param url - Geforderte Datei
 */
void httpResponseLoadWebfile (HttpResponse *response, const char *url)
{
    String *path = stringCreate(webfileDirectory);
    stringAppend(path, url);

    if (!httpResponseCheckFilePath(response, path)) {
        return;
    }

    FILE *file = fopen(path->cStr, "rb");
    if (file == NULL) {
        httpResponseSetup(response, HTTP_STATUS_NOT_FOUND,
                          0, NULL, 0);
        stringFree(path);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    stringReserve(response->payload, fileSize);
    fread(response->payload->cStr, fileSize, 1, file);

    fclose(file);

    // Bestimme den MIME-Typ anhand des Datei-Suffixes
    String *mimeType = stringCreate("Content-Type: ");
    const char *urlSuffix = strrchr(path->cStr, '.');
    if (urlSuffix != NULL) urlSuffix++;
    stringAppend(mimeType, getMimeType(urlSuffix));

    httpResponseSetup(response, HTTP_STATUS_OK, fileSize, NULL,
                      1, mimeType->cStr);

    stringFree(mimeType);

    stringFree(path);
}


/**
 * Prüft einen Dateipfad. Ist wahr wenn der Pfad zu einer existierenden Datei
 * innerhalb des Webfile-Verzeichnisses führt (ggf. wird 'path' dabei verändert).
 * Anderenfalls wird der Fehlerstatus im Http-Response-Objekts gesetzt.
 *
 * @param response - Zielobjekt
 * @param url - Geforderte Datei
 */
bool httpResponseCheckFilePath (HttpResponse *response, String *path)
{
    char *realWebfileDirectory = realpath(webfileDirectory, NULL);
    char *realPath = realpath(path->cStr, NULL);

    struct stat pathStat;
    if (realPath) stat(realPath, &pathStat);

    // Pfad existiert nicht (evtl. einschliesslich Webfile-Verzeichnis).
    if ((realWebfileDirectory == NULL || realPath == NULL) ||
    // Pfad befindet sich ausserhalb des Webfile-Verzeichnisses (z.B. durch "..")
        strncmp(realWebfileDirectory, realPath, strlen(realWebfileDirectory)) != 0) {
        httpResponseSetup(response, HTTP_STATUS_NOT_FOUND,
                          0, NULL, 0);
        if (realWebfileDirectory) free(realWebfileDirectory);
        if (realPath) free(realPath);
        return false;
    }

    // Pfad führt zu einem Verzeichnis
    if (S_ISDIR(pathStat.st_mode)) {
        // Wenn der Pfad ein Ordner ist aber nicht mit einem "/" endet, leite
        // den Browser auf den Pfad + "/". Das ist notwendig damit relative Links
        // korrekt vom Browser aufgelöst werden können.
        if (stringIsEmpty(path) || path->cStr[stringLength(path)-1] != '/') {
            String *location = stringCreate("Location: ");
            stringAppend(location, realPath + strlen(realWebfileDirectory));
            stringAppend(location, "/");

            httpResponseSetup(response, HTTP_STATUS_MOVED_PERMANENTLY,
                              0, NULL, 1, location->cStr);

            stringFree(location);
            free(realWebfileDirectory);
            free(realPath);
            return false;
        }

        // Antworte mit der index-Datei des Verzeichnisses
        stringAppend(path, indexWebfile);
    }

    free(realWebfileDirectory);
    free(realPath);
    return true;
}


/**
 * Formatiert ein Befehlsobjekt zu Json.
 *
 * @param cmd - Zielobjekt
 * @param json - Ausgabestring
 */
void commandToJson (Command *cmd, String *json)
{
    stringCopyFormat(json, "{\r\n"
                           "\"command\":\"%s\",\r\n"
                           "\"key\":\"%s\",\r\n"
                           "\"value\":\"%s\",\r\n"
                           "\"responseMessage\":\"%s\",\r\n"
                           "\"responseRecords\":[\r\n",
                     cmd->name->cStr, cmd->key->cStr, cmd->value->cStr,
                     cmd->responseMessage->cStr);

    size_t records = cmd->responseRecords->size;

    for (int i = 0; i < records; i++) {
        ResponseRecord *record = cmd->responseRecords->cArr[i];
        const char *key = record->key->cStr;
        const char *value = record->value->cStr;
        stringAppendFormat(json, "{\r\n"
                                 "\t\"key\":\"%s\",\r\n"
                                 "\t\"value\":\"%s\"\r\n",
                           key, value);
        stringAppend(json, (i == records - 1) ? "}\r\n" : "},\r\n");
    }

    stringAppend(json, "]\r\n}\r\n");
}


const char* getHttpStatusName (int status)
{
    static const int statusCode[] = {HTTP_STATUS_OK, HTTP_STATUS_MOVED_PERMANENTLY,
                                     HTTP_STATUS_NOT_FOUND, HTTP_STATUS_METHOD_NOT_ALLOWED,
                                     HTTP_STATUS_INTERNAL_SERVER_ERROR};
    static const char *statusName[] = {"OK", "Moved Permanently",
                                       "Not Found", "Method Not Allowed",
                                       "Internal Server Error"};

    for (int i = 0; i < sizeof(statusCode) / sizeof(int); i++) {
        if (statusCode[i] == status) {
            return statusName[i];
        }
    }
    return "";
}


const char* getMimeType (const char* fileSuffix)
{
    static const char *suffix[] = {"html", "js", "css", "jpg", "png"};
    static const char *mimeType[] = {"text/html", "text/javascript", "text/css",
                                     "image/jpeg", "image/png"};

    if (fileSuffix != NULL) {
        for (int i = 0; i < sizeof(suffix) / sizeof(char *); i++) {
            if (strcmp(fileSuffix, suffix[i]) == 0) {
                return mimeType[i];
            }
        }
    }
    return "text/plain";
}

