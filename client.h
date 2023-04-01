#include "irc.h"

struct client {
	int fd;

	char sendbuf[4096];
	char recvbuf[4096];
	int sendptr;
	int recvptr;
};

extern int clientsz;
extern int clientptr;
extern struct client *clients;

void client_readable(int fd);
void client_writable(int fd);

int client_ircf(struct client *c, const char *fmt, ...);
int client_read(struct client *c, struct irc_message *out);
