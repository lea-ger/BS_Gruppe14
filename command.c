#include "command.h"


/*
 * Befehlsverteiler
 *
 * Verantwortlich für das Interpretieren, Validieren und Ausführen eingehender Befehle,
 * sowie dem Formatieren von Antwortnachrichten.
 *
 */


static Array /* CommandEntry */ *commandTable = NULL;


void initModuleCommand ()
{
    commandTable = arrayCreate();
}


void freeModuleCommand ()
{
    freeCommandTable();
    arrayFree(commandTable);
}


/**
 * Sucht nach einem bestimmten Befehl in der Befehlstabelle.
 * Liefert NULL zurück wenn er nicht enthalten ist.
 *
 * @param name - Befehl
 */
CommandEntry* lookupCommandEntry (const char* name)
{
    for (int i = 0; i < commandTable->size; i++) {
        CommandEntry *entry = commandTable->cArr[i];
        if (stringEquals(entry->name, name)) {
            return entry;
        }
    }
    return NULL;
}


/**
 * Fügt einen neuen Befehl in die Befehlstabelle ein.
 * Bei eintritt des Befehls werden dessen Argumente validiert (Anzahl, Symbole)
 * und dann die Callback-Funktion aufgerufen.
 *
 * @param name - Befehl
 * @param argc - Anzahl der Befehlsargumente
 * @param wildcardKey - Wildcard-Symbole im Key zulassen
 * @param callback - Befehls-Funktion
 */
bool registerCommandEntry (const char* name, int argc, bool wildcardKey, void (*callback)(Command*))
{
    if (*name == '\0') {
        assert(false && "Command name can't be empty!\n");
        return false;
    }
    if (argc < 0 || argc > 2) {
        assert(false && "Only 0-2 arguments allowed!\n");
        return false;
    }
    if (callback == NULL) {
        assert(false && "Function pointer is NULL!\n");
        return false;
    }

    CommandEntry *entry = lookupCommandEntry(name);
    if (entry != NULL) {
        assert(false && "Overlapping command entry registration!\n");
        return false;
    }

    entry = malloc(sizeof(CommandEntry));
    entry->name = stringToUpper(stringCreate(name));
    entry->argc = argc;
    entry->wildcardKey = wildcardKey;
    entry->callback = callback;

    arrayPushItem(commandTable, entry);

    return true;
}


/**
 * Löscht alle Befehle aus der Befehlstabelle.
 *
 */
void freeCommandTable ()
{
    for (int i = 0; i < commandTable->size; i++) {
        CommandEntry *entry = commandTable->cArr[i];
        stringFree(entry->name);
        free(entry);
    }
    arrayClear(commandTable);
}


/**
 * Setzt alle verfügbaren Befehle in einem String zusammen.
 *
 * @param cmdMessage - Ausgabe-String
 */
void formatCommandOverviewMessage (String *cmdMessage)
{
    stringCopy(cmdMessage, "SUPPORTED_COMMANDS: ");
    if (commandTable->size > 0) {
        CommandEntry *entry = commandTable->cArr[0];
        stringAppend(cmdMessage, entry->name->cStr);
    }
    for (int i = 1; i < commandTable->size; i++) {
        CommandEntry *entry = commandTable->cArr[i];
        stringAppend(cmdMessage, ", ");
        stringAppend(cmdMessage, entry->name->cStr);
    }
}


/**
 * Erzeugt ein neues Befehlsobjekt
 *
 */
Command* commandCreate ()
{
    Command *cmd = malloc(sizeof(Command));

    cmd->name = stringCreate("");
    cmd->key = stringCreate("");
    cmd->value = stringCreate("");

    cmd->responseMessage = stringCreate("");

    cmd->responseRecords = arrayCreate();

    return cmd;
}


/**
 * Gibt den Heap-Speicher des Befehlsobjekts wieder frei.
 *
 * @param cmd - Zielobjekt
 */
void commandFree (Command* cmd)
{
    stringFree(cmd->name);
    stringFree(cmd->key);
    stringFree(cmd->value);

    stringFree(cmd->responseMessage);

    responseRecordsFree(cmd->responseRecords);
    arrayFree(cmd->responseRecords);

    free(cmd);
}


/**
 * Validiert die Argumente und ruft die in der Befehlstabelle mit dem
 * Befehl assoziierten Funktion auf.
 *
 * @param cmd - Zielobjekt
 */
bool commandExecute (Command *cmd)
{
    stringCopy(cmd->responseMessage, "");
    responseRecordsFree(cmd->responseRecords);

    CommandEntry *entry = lookupCommandEntry(cmd->name->cStr);
    if (entry == NULL) {
        return false;
    }

    int argc = 0;
    if (!stringIsEmpty(cmd->key)) argc++;
    if (!stringIsEmpty(cmd->value)) argc++;
    if (argc < entry->argc) {
        stringCopy(cmd->responseMessage, "argument_missing");
        return false;
    }

    if (!stringMatchAllChar(cmd->key,
                            (entry->wildcardKey) ? "?*" : "", STR_MATCH_ALNUM)) {
        stringCopy(cmd->responseMessage, "argument_bad_symbol");
        return false;
    }

    if (entry->callback == NULL) {
        return false;
    }
    entry->callback(cmd);

    return true;
}


/**
 * Interpretiert eine Eingangsnachricht und setzt die entsprechenden
 * Attribute in das Befehlsobjekt ein.
 *
 * @param cmd - Zielobjekt
 * @param inputMessage - Eingangsnachricht
 */
void commandParseInputMessage (Command *cmd, String *inputMessage)
{
    stringTrimSpaces(inputMessage);

    const char *name = strtok(inputMessage->cStr, " \t");
    const char *key = strtok(NULL, " \t");
    const char *value = strtok(NULL, "");

    stringToUpper(stringCopy(cmd->name, (name != NULL) ? name : ""));
    stringCopy(cmd->key, (key != NULL) ? key : "");
    stringCopy(cmd->value, (value != NULL) ? value : "");
}


/**
 * Formatiert eine Ausgangsnachricht aus dem Befehlsobjekt.
 *
 * @param cmd - Zielobjekt
 * @param responseMessage - Ausgangsnachricht
 */
void commandFormatResponseMessage (const Command *cmd, String *responseMessage)
{
    CommandEntry *entry = lookupCommandEntry(cmd->name->cStr);
    if (entry == NULL) {
        formatCommandOverviewMessage(responseMessage);
        stringAppend(responseMessage, "\r\n");
        return;
    }

    size_t records = cmd->responseRecords->size;
    if (records > 0) {
        stringCopy(responseMessage, "");
        for (int i = 0; i < records; i++) {
            ResponseRecord *record = cmd->responseRecords->cArr[i];
            stringAppendFormat(responseMessage, "%s:%s:%s\r\n",
                               cmd->name->cStr, record->key->cStr, record->value->cStr);
        }
        return;
    }

    // TODO: Mehr Kontrolle bei der Ausgabe? Formatierungsflags? Errorcodes?
    stringCopy(responseMessage, cmd->name->cStr);
    if (!stringIsEmpty(cmd->key)) {
        stringAppend(responseMessage, ":");
        stringAppend(responseMessage, cmd->key->cStr);
    }
    if (!stringIsEmpty(cmd->value)) {
        stringAppend(responseMessage, ":");
        stringAppend(responseMessage, cmd->value->cStr);
    }
    if (!stringIsEmpty(cmd->responseMessage)) {
        stringAppend(responseMessage, ":");
        stringAppend(responseMessage, cmd->responseMessage->cStr);
    }
    stringAppend(responseMessage, "\r\n");
}


/**
 * Fügt einem Befehlsobjekt einen Antwort-Datensatz hinzu.
 *
 * @param records - Antwort-Datensätze
 * @param key - Schlüssel
 * @param value - Wert
 */
ResponseRecord* responseRecordsAdd (Array* records, const char* key, const char* value)
{
    ResponseRecord *record = malloc(sizeof(ResponseRecord));
    record->key = stringCreate(key);
    record->value = stringCreate(value);

    arrayPushItem(records, record);

    return record;
}


/**
 * Gibt den Heap-Speicher aller Datensätze (ResponseRecord Objekte) in dem
 * Befehlsobjekt frei.
 *
 * @param records - Antwort-Datensätze
 */
void responseRecordsFree (Array* records)
{
    for (int i = 0; i < records->size; i++) {
        ResponseRecord *record = records->cArr[i];
        stringFree(record->key);
        stringFree(record->value);
        free(record);
    }
    arrayClear(records);
}

