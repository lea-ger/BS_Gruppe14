#include "dynString.h"


/**
 * Erzeugt einen neuen String auf dem Heap-Speicher und legt
 * die anfängliche Zeichenkapazität fest. Der Initialisierungswert
 * kann die gewählte Kapazität nicht überschreiten.
 *
 * @param value - Initialisierungswert
 * @param capacity - Zeichenkapazität
 */
String* stringCreateWithCapacity (const char *value, size_t capacity)
{
    String *str = malloc(sizeof(String));

    str->cStr = malloc((capacity+1) * sizeof(char));
    str->cStr[capacity] = '\0';
    str->capacity = capacity;

    strncpy(str->cStr, value, capacity);

    return str;
}


/**
 * Erzeugt einen neuen String auf dem Heap-Speicher.
 *
 * @param value - Initialisierungswert
 */
String* stringCreate (const char *value)
{
    size_t capacity = strlen(value);
    if (capacity < INITIAL_STRING_CAPACITY) {
        capacity = INITIAL_STRING_CAPACITY;
    }
    return stringCreateWithCapacity(value, capacity);
}


/**
 * Erzeugt einen neuen String auf dem Heap-Speicher und
 * initialisiert ihn mit Formatierungshinweisen und Einsatzwerten.
 *
 * @param format - Formatierungshinweise
 * @param ... - Einsatzwerte
 */
String* stringCreateWithFormat (const char *format, ...)
{
    va_list vaList;

    va_start(vaList, format);
    int capacity = vsnprintf(NULL, 0, format, vaList);
    va_end(vaList);
    if (capacity < INITIAL_STRING_CAPACITY) {
        capacity = INITIAL_STRING_CAPACITY;
    }

    String *str = stringCreateWithCapacity("", capacity);

    va_start(vaList, format);
    vsnprintf(str->cStr, capacity+1, format, vaList);
    va_end(vaList);

    return str;
}


/**
 * Gibt dem Heap-Speicher des Strings wieder frei.
 *
 * @param str - Zielobjekt
 */
void stringFree (String *str)
{
    free(str->cStr);
    free(str);
}


/**
 * Verändert die Kapazität auf dem Heap-Speicher.
 * 'capacity' entspricht der Anzahl der Zeichen die das Zielobjekt fassen kann.
 *
 * @param str - Zielobjekt
 * @param capacity - Zeichenkapazität
 */
void stringReserve (String *str, size_t capacity)
{
    str->cStr = realloc(str->cStr, (capacity+1) * sizeof(char));
    str->cStr[capacity] = '\0';
    str->capacity = capacity;
}


/**
 * Reduziert die Kapazität des Heap-Speicher auf die Länge der Zeichenkette
 * des Zielobjekts.
 *
 * @param str - Zielobjekt
 */
void stringShrinkToFit (String *str)
{
    stringReserve(str, stringLength(str));
}


void stringAdjustCapacity (String *str, size_t newStrLen)
{
    if (newStrLen > str->capacity) {
        size_t newCapacity = str->capacity * 2;
        if (newStrLen > newCapacity) {
            newCapacity = newStrLen;
        }
        stringReserve(str, newCapacity);
    }
}


/**
 * Gibt die Länge der Zeichenkette zurück.
 *
 * @param str - Zielobjekt
 * @return Länge der Zeichenkette
 */
size_t stringLength (const String *str)
{
    return strlen(str->cStr);
}


/**
 * Wahr wenn die Zeichenkette die Länge 0 hat.
 *
 * @param str - Zielobjekt
 */
bool stringIsEmpty (const String* str)
{
    return str->cStr[0] == '\0';
}


/**
 * Prüft auf Übereinstimmung aller Zeichen.
 *
 * @param str - Zielobjekt
 * @param value - Vergleich-String
 */
bool stringEquals (const String *str, const char* value)
{
    return strcmp(str->cStr, value) == 0;
}


/**
 * Kopiert eine Zeichenkette in den Heap-Speicher des Zielobjekts.
 *
 * @param str - Zielobjekt
 * @param value - Zeichenkette
 */
String* stringCopy (String *str, const char *value)
{
    size_t newStrLen = strlen(value);
    stringAdjustCapacity(str, newStrLen);

    strcpy(str->cStr, value);

    return str;
}


/**
 * Konkateniert eine Zeichenkette mit der Zeichenkette des Zielobjekts.
 *
 * @param str - Zielobjekt
 * @param value - Zeichenkette
 */
String* stringAppend (String *str, const char *value)
{
    size_t newStrLen = strlen(str->cStr) + strlen(value);
    stringAdjustCapacity(str, newStrLen);

    strcat(str->cStr, value);

    return str;
}


/**
 * Kopiert eine Zeichenkette in den Heap-Speicher des Zielobjekts
 * und benutzt dabei Formatierungshinweise und Einsatzwerte.
 *
 * @param str - Zielobjekt
 * @param format - Formatierungshinweise
 * @param ... - Einsatzwerte
 */
String* stringCopyFormat (String *str, const char *format, ...)
{
    va_list vaList;

    va_start(vaList, format);
    int newStrLen = vsnprintf(NULL, 0, format, vaList);
    va_end(vaList);

    stringAdjustCapacity(str, newStrLen);

    va_start(vaList, format);
    vsnprintf(str->cStr, newStrLen+1, format, vaList);
    va_end(vaList);

    return str;
}


/**
 * Konkateniert eine Zeichenkette mit der Zeichenkette des Zielobjekts
 * und benutzt dabei Formatierungshinweise und Einsatzwerte.
 *
 * @param str - Zielobjekt
 * @param format - Formatierungshinweise
 * @param ... - Einsatzwerte
 */
String* stringAppendFormat (String *str, const char *format, ...)
{
    size_t strLen = strlen(str->cStr);

    va_list vaList;

    va_start(vaList, format);
    size_t newStrLen = strLen + vsnprintf(NULL, 0, format, vaList);
    va_end(vaList);

    stringAdjustCapacity(str, newStrLen);

    va_start(vaList, format);
    vsnprintf(&str->cStr[strLen], newStrLen-strLen+1, format, vaList);
    va_end(vaList);

    return str;
}


/**
 * Ersetzt alle Buchstaben in der Zeichenkette des Zielobjekts mit Großbuchstaben
 *
 * @param str - Zielobjekt
 */
String* stringToUpper (String *str)
{
    strToUpper(str->cStr);
    return str;
}


/**
 * Ersetzt alle Buchstaben in der Zeichenkette des Zielobjekts mit Kleinbuchstaben
 *
 * @param str - Zielobjekt
 */
String* stringToLower (String *str)
{
    strToLower(str->cStr);
    return str;
}


/**
 * Schneidet einen Teilzeichenkette aus.
 * Beginnend bei Index 0 fängt die neue Zeichenkette bei 'start' an
 * und endet vor 'end'.
 *
 * Beispiel: "Teilzeichenkette" start=4 end=11 -> "zeichen"
 *
 * @param str - Zielobjekt
 * @param start - Anfangsindex (inklusiv)
 * @param end - Schlussindex (exklusiv)
 */
String* stringCut (String *str, size_t start, size_t end)
{
    strCut(str->cStr, start, end);
    return str;
}


/**
 * Entfernt alle Whitespace-characters am Anfang und am Ende
 * der Zeichenkette des Zielobjekts.
 *
 * @param str - Zielobjekt
 */
String* stringTrimSpaces (String *str)
{
    strTrimSpaces(str->cStr);
    return str;
}


/**
 * Prüft ob ALLE Zeichen des Zielobjekts einem der Zeichen in "match" entsprechen,
 * oder einer durch das Bit-flag "charGroup" ausgewählten Zeichen-Gruppe zugehören.
 *
 * @param str - Zielobjekt
 * @param match - Vergleichszeichen
 * @param charGroup - Zeichen-Gruppe
 */
bool stringMatchAllChar (const String *str, const char *match, int charGroup)
{
    return strMatchAllChar(str->cStr, match, charGroup);
}


/**
 * Prüft ob EINES der Zeichen des Zielobjekts einem der Zeichen in "match" entspricht,
 * oder einer durch das Bit-flag "charGroup" ausgewählten Zeichen-Gruppe zugehört.
 * Liefert den Index des ersten Matches zurück, anderenfalls -1.
 *
 * @param str - Zielobjekt
 * @param match - Vergleichszeichen
 * @param charGroup - Zeichen-Gruppe
 */
int stringMatchAnyChar (const String *str, const char *match, int charGroup)
{
    return strMatchAnyChar(str->cStr, match, charGroup);
}


/**
 * Vergleicht die Zeichenkette des Zielobjekts mit der Zeichenkette 'wildcard'.
 * 'wildcard' kann die Platzhalter-Symbole '?' (genau ein Zeichen)
 *  und '*' (kein oder beliebig viele Zeichen) enthalten.
 *
 * @param str - Zielobjekt
 * @param wildcard - Wildcard
 */
bool stringMatchWildcard (const String* str, const char *wildcard)
{
    return strMatchWildcard(str->cStr, wildcard);
}


char* strToUpper (char *str)
{
    for (; *str != '\0'; str++) {
        *str = (char)toupper(*str);
    }
    return str;
}


char* strToLower (char *str)
{
    for (; *str != '\0'; str++) {
        *str = (char)tolower(*str);
    }
    return str;
}


char* strCut (char* str, size_t start, size_t end)
{
    if (end < strlen(str) && end >= 0) {
        str[end] = '\0';
    }

    if (start > 0 && start <= strlen(str)) {
        for (int i = 0; str[i + start] != '\0'; i++) {
            str[i] = str[i + start];
        }
        str[end - start] = '\0';
    }

    return str;
}


char* strTrimSpaces (char *str)
{
    size_t trailing = strlen(str);
    while (trailing > 0 && isspace(str[trailing-1])) trailing--;

    size_t leading = 0;
    while (isspace(str[leading])) leading++;

    return strCut(str, leading, trailing);
}


bool strMatchAllChar (const char *str, const char *match, int charGroup)
{
    bool lower = charGroup & STR_MATCH_LOWER;
    bool upper = charGroup & STR_MATCH_UPPER;
    bool digit = charGroup & STR_MATCH_DIGIT;

    for (char c = *str; c != '\0'; c = *(++str)) {
        if (strchr(match, c) == NULL &&
                (!lower || !islower(c)) &&
                (!upper || !isupper(c)) &&
                (!digit || !isdigit(c))) {
            return false;
        }
    }

    return true;
}


int strMatchAnyChar (const char* str, const char *match, int charGroup)
{
    bool lower = charGroup & STR_MATCH_LOWER;
    bool upper = charGroup & STR_MATCH_UPPER;
    bool digit = charGroup & STR_MATCH_DIGIT;

    for (char c = *str, index = 0; c != '\0'; c = *(++str), index++) {
        if (strchr(match, c) != NULL ||
            (lower && islower(c)) ||
            (upper && isupper(c)) ||
            (digit && isdigit(c))) {
            return index;
        }
    }

    return -1;
}


// Kryptische Original-Version: https://www.ioccc.org/2001/schweikh.c
bool strMatchWildcard (const char *str, const char *wildcard)
{
    if (*wildcard == '*') {
        // Wenn der Wildcard-Ausdruck mit einem '*' endet, ist
        // jedes weitere Zeichen im String immer ein Match
        if (*(wildcard+1) == '\0' ||
                // '*' ist nichts im String (oder wurde bereits übersprungen)
                strMatchWildcard(str, wildcard+1) ||
                // '*' ist ein (oder mehrere) Zeichen im String
                (*str != '\0' && strMatchWildcard(str+1, wildcard))) {
            return true;
        }
        return false;
    }

    // Wenn der String am Ende ist, muss der Wildcard-Ausdruck es auch sein
    // (und aktuell kein '*', wurde bereits ausgeschlossen), sonst haben wir
    // ein Mismatch
    if (*str == '\0') {
        return *wildcard == '\0';
    }

    // Normaler 1:1 bzw. 1:'?' Vergleich schlägt fehl
    if (*str != *wildcard && *wildcard != '?') {
        return false;
    }

    return strMatchWildcard(str+1, wildcard+1);
}

