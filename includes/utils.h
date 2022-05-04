#ifndef UTILS_H
#define UTILS_H

#include "dynString.h"
#include "dynArray.h"

#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>


#define PAGE_SIZE 4096


void freeResourcesAndExit();
void fatalError(const char *message);


#endif //UTILS_H
