#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "client.h"
#include "log.h"

int clientsz = 32;
int clientptr = -1;
struct client *clients;

static struct client *
find_client(int fd)
{
	for (int i = 0; i <= clientptr; ++i) {
		if (clients[i].fd == fd)
			return &clients[i];
	}
	return NULL;
}

void
client_readable(int fd)
{
	struct client *c = find_client(fd);
	assert(c != NULL);

	// Read as much as we can
	int n = read(fd, c->recvbuf+c->recvptr, sizeof(c->recvbuf)-c->recvptr);
	if (n == -1) {
		// Hangup!
		warnf("failed reading from client fd %d: %s", fd, strerror(errno));
		close(fd);
		return;
	}

	c->recvptr += n;

	// Try to find a message
	char *nl = memchr(c->recvbuf, '\n', c->recvptr);
	if (nl == NULL) // Nothing yet
		return;

	// Set zeroes
	if (nl-(c->recvbuf) > 1 && *(nl - 1) == '\r')
		*(nl - 1) = 0;
	else
		*(nl) = 0;

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(c->recvbuf, &msg)) {
		warnf("Failed to parse IRC message from client fd %d. Disconnecting.", fd);
		close(fd);
	}

	debugf("%d RX command: %s", fd, msg.command);

	// Reset buffer
	memmove(c->recvbuf, nl+1, c->recvptr - (nl - c->recvbuf));
	c->recvptr -= (nl - c->recvbuf)+1;
}

void
client_writable(int fd)
{

}
