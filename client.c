#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "log.h"
#include "main.h"

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

/* client_sendf sends a formatted response (ideally like IRC) to the client
 * The \r\n delimiters are automatically appended.
 *
 * The number of bytes written to the send buffer is returned.
 */
int
client_sendf(struct client *c, const char *fmt, ...)
{
	// Find our pfd
	for (int i = 0; i <= pollfdptr; ++i) {
		if (pollfds[i].fd == c->fd) {
			// Add POLLOUT
			pollfds[i].events |= POLLOUT;
			break;
		}
	}

	// Chuck stuff onto the buffer
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(c->sendbuf+c->sendptr, sizeof(c->sendbuf)-c->sendptr, fmt, ap);
	va_end(ap);
	
	debugf("%d >> %s", c->fd, c->sendbuf+c->sendptr);

	n += snprintf(c->sendbuf+c->sendptr+n, sizeof(c->sendbuf)-c->sendptr-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.
	assert(c->sendptr + n < sizeof(c->sendbuf));

	c->sendptr += n;

	return n;
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

	// TODO: Handle overful scenarios

	// Try to find a message
	char *nl = memchr(c->recvbuf, '\n', c->recvptr);
	if (nl == NULL) // Nothing yet
		return;

	// Set zeroes
	if (nl-(c->recvbuf) > 1 && *(nl - 1) == '\r')
		*(nl - 1) = 0;
	else
		*(nl) = 0;
	
	debugf("%d << %s", fd, c->recvbuf);

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(c->recvbuf, &msg)) {
		warnf("Failed to parse IRC message from client fd %d. Disconnecting.", fd);
		close(fd);
	}

	// Reset buffer
	memmove(c->recvbuf, nl+1, c->recvptr - (nl - c->recvbuf));
	c->recvptr -= (nl - c->recvbuf)+1;
}

void
client_writable(int fd)
{
	struct client *c = find_client(fd);
	assert(c != NULL);

	int n = write(fd, c->sendbuf, c->sendptr);

	// Move everything back
	memmove(c->sendbuf, c->sendbuf+c->sendptr, c->sendptr-n);
	c->sendptr -= n;

	if (c->sendptr == 0) {
		// Remove POLLOUT if we have nothing more to send
		for (int i = 0; i <= pollfdptr; ++i) {
			if (pollfds[i].fd == c->fd) {
				pollfds[i].events ^= POLLOUT;
				pollfds[i].revents ^= POLLOUT;
				break;
			}
		}
	}
}
