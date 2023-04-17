#include "irc.h"
#include "vec.h"

extern int ircfd;
extern struct mca_vector server_isupport;

int server_readable(void);
void server_writable(void);

int server_sendf(const char *fmt, ...);
int server_sendmsg(struct irc_message *msg);
