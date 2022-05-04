#ifndef DYNSTRING_H
#define DYNSTRING_H

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


#define INITIAL_STRING_CAPACITY 15

#define STR_MATCH_NOGROUP 0
#define STR_MATCH_LOWER 1
#define STR_MATCH_UPPER 2
#define STR_MATCH_ALPHA 3
#define STR_MATCH_DIGIT 4
#define STR_MATCH_ALNUM 7


typedef struct {
    char *cStr;
    size_t length;
    size_t capacity;
} String;


String* stringCreate (const char* value);
String* stringCreateWithFormat (const char* format, ...);
String* stringCreateWithCapacity (const char* value, size_t capacity);
void stringFree (String* str);
void stringReserve (String* str, size_t capacity);
void stringShrinkToFit (String* str);

size_t stringLength (String* str);
bool stringIsEmpty (const String* str);
bool stringEquals (const String* str, const char* value);

String* stringCopy (String* str, const char* value);
String* stringAppend (String* str, const char* value);
String* stringCopyFormat (String* str, const char* format, ...);
String* stringAppendFormat (String* str, const char* format, ...);

String* stringToUpper (String* str);
String* stringToLower (String* str);
String* stringCut (String* str, size_t start, size_t end);
String* stringTrimSpaces (String* str);

bool stringMatchAllChar (const String* str, const char *match, int charGroup);
int stringMatchAnyChar (const String* str, const char *match, int charGroup);
bool stringMatchWildcard (const String* str, const char *wildcard);

char* strToUpper (char *str);
char* strToLower (char *str);
char* strCut (char* str, size_t start, size_t end);
char* strTrimSpaces (char *str);

bool strMatchAllChar (const char *str, const char *match, int charGroup);
int strMatchAnyChar (const char *str, const char *match, int charGroup);
bool strMatchWildcard (const char *str, const char *wildcard);


#endif // DYNSTRING_H
