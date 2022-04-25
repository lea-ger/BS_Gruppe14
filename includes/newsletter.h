#ifndef SERVER_NEWSLETTER_H
#define SERVER_NEWSLETTER_H

#include "utils.h"
#include "command.h"


void eventCommandSubscribe (Command *cmd);

void initModulNewsletter ();
void freeModulNewsletter ();


#endif //SERVER_NEWSLETTER_H
