#include <stddef.h>

struct irc_message {
	char *tags;
	char *source;
	char *command;
	char *params[15]; // XXX: Should be more according to IRC v3.	
};

int irc_parse(char *msg, struct irc_message *out);
