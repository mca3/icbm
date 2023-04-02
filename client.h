#include "irc.h"
#include "bufio.h"

struct client {
	int fd;
	struct bufio b;
};

extern int clientsz;
extern int clientptr;
extern struct client *clients;

void client_readable(int fd);
void client_writable(int fd);

int client_sendf(struct client *c, const char *fmt, ...);
