#include "irc.h"

extern int ircfd;

void server_readable(void);
void server_writable(void);

int server_ircf(const char *fmt, ...);
int server_read(struct irc_message *out);
