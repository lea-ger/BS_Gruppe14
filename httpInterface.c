#include "httpInterface.h"


/*
 * Ein einfacher Webserver.
 *
 * Hostet Webfiles zum Abrufen für einen Browser.
 * Stellt über eine bestimmte URL eine REST-API zur Datenbank bereit
 * (GET, PUT, DELETE; Ergebnisse im JSON-Format).
 *
 */


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

    response->statusCode = 0;
    response->payloadSize = 0;

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
 * Interpretiert eine Http-Anfrage und setzt die entsprechenden
 * Attribute in das Http-Request-Objekt ein.
 *
 * @param request - Zielobjekt
 * @param requestMessage - Http-Anfrage
 */
void httpRequestParseMessage (HttpRequest *request, String *requestMessage)
{
    const char* payload = strstr(requestMessage->cStr, "\r\n\r\n");
    const char* method = strtok(requestMessage->cStr, " \t");
    const char* url    = strtok(NULL, " \t");

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
    if (strncmp(request->url->cStr, STORAGE_URL, strlen(STORAGE_URL)) == 0) {
        if (!stringEquals(request->method, "GET") &&
            !stringEquals(request->method, "PUT") &&
            !stringEquals(request->method, "DELETE")) {
            response->statusCode = HTTP_STATUS_METHOD_NOT_ALLOWED;
            httpResponseAttributeAdd(response, "Allow: GET, PUT, DELETE");
            return;
        }

        Command *cmd = commandCreate();

        stringCopy(cmd->name, request->method->cStr);
        stringCopy(cmd->key, request->url->cStr);
        stringCopy(cmd->value, request->payload->cStr);

        // "DELETE" -> "DEL"
        if (stringEquals(cmd->name, "DELETE")) {
            stringCut(cmd->name, 0, 3);
        }
        // "/storage/key" -> "key"
        stringCut(cmd->key,strlen(STORAGE_URL),
                  stringLength(cmd->key));

        commandExecute(cmd);
        commandToJson(cmd, response->payload);

        commandFree(cmd);

        response->statusCode = HTTP_STATUS_OK;
        response->payloadSize = stringLength(response->payload);
        httpResponseAttributeAdd(response, "Content-Type: application/json");
        return;
    }
    // Zugriff auf das Webfile-Verzeichnis
    // -----------------------------------
    if (!stringEquals(request->method, "GET")) {
        response->statusCode = HTTP_STATUS_METHOD_NOT_ALLOWED;
        httpResponseAttributeAdd(response, "Allow: GET");
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
void httpResponseFormateMessage (HttpResponse *response, String *responseMessage, size_t *responseMessageSize)
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
        httpResponseAttributeAdd(response, "Content-Type: text/html");
    }

    stringAppendFormat(responseMessage, "Content-Length: %d\r\n", response->payloadSize);

    for (int i = 0; i < response->attributes->size; i++) {
        String *attribut = response->attributes->cArr[i];
        stringAppend(responseMessage, attribut->cStr);
        stringAppend(responseMessage, "\r\n");
    }
    stringAppend(responseMessage, "\r\n");

    *responseMessageSize = stringLength(responseMessage) + response->payloadSize;

    stringReserve(responseMessage, *responseMessageSize);
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
    String *path = stringCreate(WEB_ROOT_DIR);
    stringAppend(path, url);

    if (!httpResponseCheckFilePath(response, path)) {
        return;
    }

    FILE *file = fopen(path->cStr, "rb");
    if (file == NULL) {
        response->statusCode = HTTP_STATUS_NOT_FOUND;
        stringFree(path);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    stringReserve(response->payload, fileSize);
    fread(response->payload->cStr, fileSize, 1, file);

    fclose(file);

    response->statusCode = HTTP_STATUS_OK;
    response->payloadSize = fileSize;

    // Versuche den MIME-Typ anhand des Datei-Suffixes zu bestimmen
    const char *urlSuffix = strrchr(path->cStr, '.');
    if (urlSuffix != NULL) urlSuffix++;
    const char *mimeType = getMimeType(urlSuffix);

    if (mimeType != NULL) {
        String *contentTypeAttribute = stringCreateWithFormat("Content-Type: %s", mimeType);
        httpResponseAttributeAdd(response, contentTypeAttribute->cStr);
        stringFree(contentTypeAttribute);
    }

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
    char *realWebfileDirectory = realpath(WEB_ROOT_DIR, NULL);
    char *realPath = realpath(path->cStr, NULL);

    struct stat pathStat;
    if (realPath) stat(realPath, &pathStat);

    // Pfad existiert nicht (evtl. einschliesslich Webfile-Verzeichnis).
    if ((realWebfileDirectory == NULL || realPath == NULL) ||
    // Pfad befindet sich ausserhalb des Webfile-Verzeichnisses (z.B. durch "..")
        strncmp(realWebfileDirectory, realPath, strlen(realWebfileDirectory)) != 0) {
        response->statusCode = HTTP_STATUS_NOT_FOUND;

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

            response->statusCode = HTTP_STATUS_MOVED_PERMANENTLY;
            httpResponseAttributeAdd(response, location->cStr);

            stringFree(location);

            free(realWebfileDirectory);
            free(realPath);
            return false;
        }

        // Antworte mit der index-Datei des Verzeichnisses
        stringAppend(path, WEB_INDEX_FILE);
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
    size_t records = cmd->responseRecords->size;

    stringCopyFormat(json, "{\r\n"
                           "\"command\":\"%s\",\r\n"
                           "\"key\":\"%s\",\r\n"
                           "\"value\":\"%s\",\r\n"
                           "\"responseMessage\":\"%s\",\r\n"
                           "\"responseRecordsSize\":%d,\r\n"
                           "\"responseRecords\":[\r\n",
                     cmd->name->cStr, cmd->key->cStr, cmd->value->cStr,
                     cmd->responseMessage->cStr, records);

    for (int i = 0; i < records; i++) {
        ResponseRecord *record = cmd->responseRecords->cArr[i];
        stringAppendFormat(json, "{\r\n"
                                 "\t\"key\":\"%s\",\r\n"
                                 "\t\"value\":\"%s\"\r\n",
                           record->key->cStr, record->value->cStr);
        stringAppend(json, (i == records - 1) ? "}\r\n" : "},\r\n");
    }

    stringAppend(json, "]\r\n}\r\n");
}


/**
 * Fügt einem Http-Response-Objekt ein zusätzliches Attribut hinzu.
 *
 * @param response - Zielobjekt
 * @param attribute - HTTP Attribut
 */
void httpResponseAttributeAdd (HttpResponse *response, const char* attribute)
{
    arrayPushItem(response->attributes, stringCreate(attribute));
}


/**
 * Gibt den Heap-Speicher aller zusätzlicher Attribute (String Objekte) in dem
 * Http-Response-Objekt frei.
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
    static const char *suffix[] = {"html", "js", "css",
                                   "jpg", "png", "gif", "svg"};
    static const char *mimeType[] = {"text/html", "text/javascript", "text/css",
                                     "image/jpeg", "image/png",
                                     "image/gif", "image/svg+xml"};

    if (fileSuffix != NULL) {
        for (int i = 0; i < sizeof(suffix) / sizeof(char *); i++) {
            if (strcmp(fileSuffix, suffix[i]) == 0) {
                return mimeType[i];
            }
        }
    }
    return NULL;
}

