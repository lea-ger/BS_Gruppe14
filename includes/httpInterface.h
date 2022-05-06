#ifndef SERVER_HTTPINTERFACE_H
#define SERVER_HTTPINTERFACE_H

#include "utils.h"
#include "command.h"

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>


#define HTTP_STATUS_OK 200
#define HTTP_STATUS_MOVED_PERMANENTLY 301
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405
#define HTTP_STATUS_INTERNAL_SERVER_ERROR 500

#define WEB_ROOT_DIR "../http/"
#define WEB_INDEX_FILE "index.html"
#define STORAGE_URL "/storage/"


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

void httpRequestParseMessage (HttpRequest *request, String *requestMessage);
void httpRequestProcess (HttpRequest *request, HttpResponse *response);
void httpResponseFormateMessage (HttpResponse *response, String *responseMessage, size_t *responseMessageSize);

void httpResponseLoadWebfile (HttpResponse *response, const char *url);
bool httpResponseCheckFilePath (HttpResponse *response, String *path);
void commandToJson (Command *cmd, String *json);

void httpResponseAttributeAdd (HttpResponse *response, const char* attribute);
void httpResponseAttributesFree (HttpResponse *response);

const char* getHttpStatusName (int status);
const char* getMimeType (const char* fileSuffix);


#endif //SERVER_HTTPINTERFACE_H
