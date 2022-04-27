#ifndef COMMAND_H
#define COMMAND_H

/*
 * Befehlsverteiler
 *
 * Verantwortlich für das Interpretieren, Validieren und Ausführen eingehender Befehle,
 * sowie dem Formatieren von Antwortnachrichten.
 *
 */

#include "dynString.h"
#include "dynArray.h"

#include <assert.h>


#define DELIMITER " "


typedef struct {
    String *name;
    String *key;
    String *value;
    String *responseMessage;
    Array /* ResponseRecord */ *responseRecords;
} Command;


typedef struct {
    String *name;
    int argc;
    bool wildcardKey;
    void (*callback)(Command*);
} CommandEntry;


typedef struct {
    String *key;
    String *value;
} ResponseRecord;


void initModulCommand ();
void freeModulCommand ();

CommandEntry* lookupCommandEntry (const char* name);
bool registerCommandEntry (const char* name, int argc, bool wildcardKey, void (*callback)(Command*));
void freeCommandTable ();
void formatCommandOverviewMessage (String *cmdMessage);

Command* commandCreate ();
void commandFree (Command* cmd);

bool commandExecute (Command *cmd);
void commandParseInputMessage (Command *cmd, String *inputMessage);
void commandFormatResponseMessage (const Command *cmd, String *responseMessage);

ResponseRecord* responseRecordsAdd (Array* records, const char* key, const char* value);
void responseRecordsFree (Array* records);


#endif //COMMAND_H
