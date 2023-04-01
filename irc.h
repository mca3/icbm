#ifndef INC_IRC_H
#define INC_IRC_H
#include <stddef.h>

struct irc_message {
	char *tags;
	char *source;
	char *command;
	char *params[15]; // XXX: Should be more according to IRC v3.	
};

extern int ircfd; /* Defined in main.c */

int irc_parse(char *msg, struct irc_message *out);
#endif
