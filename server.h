#include "irc.h"

extern int ircfd;

int server_readable(void);
void server_writable(void);

int server_sendf(const char *fmt, ...);
