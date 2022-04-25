#ifndef SERVER_HTTPINTERFACE_H
#define SERVER_HTTPINTERFACE_H

/*
 * Ein einfacher Webserver.
 *
 * Hostet Webfiles zum Abrufen für einen Browser.
 * Stellt über eine bestimmte URL eine REST-API zur Datenbank bereit
 * (GET, PUT, DELETE; Ergebnisse im JSON-Format).
 *
 */

#include "network.h"

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>


#define HTTP_STATUS_OK 200
#define HTTP_STATUS_MOVED_PERMANENTLY 301
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405
#define HTTP_STATUS_INTERNAL_SERVER_ERROR 500


static const char* webfileDirectory = "../http/";
static const char* indexWebfile = "index.html";
static const char* storageUrl = "/storage/";


typedef struct {
    String *method;
    String *url;
    String *payload;
} HttpRequest;


typedef struct {
    int statusCode;
    Array /* String */ *attributes;
    size_t payloadSize;
    String *payload;
} HttpResponse;


HttpRequest* httpRequestCreate ();
void httpRequestFree (HttpRequest *request);
HttpResponse* httpResponseCreate ();
void httpResponseFree (HttpResponse *response);

void httpResponseSetup (HttpResponse *response, int statusCode,
                        size_t payloadSize, const char *payload, int additionalAttributes, ...);
void httpResponseAttributesFree (HttpResponse *response);

void httpRequestParseMessage (HttpRequest *request, String *requestMessage);
void httpRequestProcess (HttpRequest *request, HttpResponse *response);
void httpResponseFormateMessage (HttpResponse *response, String *responseMessage);

void httpResponseLoadWebfile (HttpResponse *response, const char *url);
bool httpResponseCheckFilePath (HttpResponse *response, String *path);
void commandToJson (Command *cmd, String *json);

const char* getHttpStatusName (int status);
const char* getMimeType (const char* fileSuffix);


#endif //SERVER_HTTPINTERFACE_H
