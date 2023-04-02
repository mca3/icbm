#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "evloop.h"
#include "log.h"

int clientsz = 8;
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
 * The number of bytes written to the send buffer is returned, or -1 upon
 * failure.
 */
int
client_sendf(struct client *c, const char *fmt, ...)
{
	char buf[2048];

	// Tell the event loop we want to write out
	ev_set_writeout(c->fd, 1);

	// Chuck stuff onto the buffer
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	debugf("%d >> %s", c->fd, buf);

	n += snprintf(buf+n, sizeof(buf)-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.

	return bufio_write(&c->b, buf, n);
}

int
client_readable(int fd)
{
	int n;
	struct client *c = find_client(fd);
	assert(c != NULL);

	if ((n = bufio_readable(&c->b, fd)) == -1) {
		warnf("failed reading from client fd %d: %s", fd, strerror(errno));
		close(ircfd);
		return 0;
	}

	if (!n) // Partial read
		return 0;
	
	debugf("%d << %s", fd, c->b.recvbuf);

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(c->b.recvbuf, &msg)) {
		warnf("Failed to parse IRC message from client fd %d. Disconnecting.", fd);
		close(fd);
		return 0;
	}

	return 1;
}

void
client_writable(int fd)
{
	struct client *c = find_client(fd);
	assert(c != NULL);

	int n = bufio_writable(&c->b, fd);

	if (n > 0) {
		// Tell the event loop we no longer want to write out
		ev_set_writeout(fd, 0);
	} else if (n == -1) {
		warnf("Write failed to fd %d: %s", fd, strerror(errno));
		close(fd);
	}
}
