#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>

#include "bufio.h"
#include "client.h"
#include "evloop.h"
#include "irc.h"
#include "log.h"
#include "server.h"

static struct bufio server_bufio = {0};

static void
server_client_forward(struct irc_message *msg)
{
	// Write the message to a buffer first
	char buf[2048];
	int n;

	if ((n = irc_string(msg, buf, sizeof(buf))) == -1)
		return;

	// Tell the event loop we want to write out
	ev_set_writeout(ircfd, 1);

	// Chuck stuff onto the buffer
	debugf("* >> %s", buf);

	n += snprintf(buf+n, sizeof(buf)-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.

	// Send to all clients
	// TODO: Only those whom are authenticated
	for (int i = 0; i <= clientptr; ++i) {
		// TODO: This should be a client_... function.
		ev_set_writeout(clients[i].fd, 1);
		bufio_write(&clients[i].b, buf, n);
	}
}

/* server_sendf sends a formatted response (ideally like IRC) to the server
 * The \r\n delimiters are automatically appended.
 *
 * The number of bytes written to the send buffer is returned, or -1 upon
 * failure.
 */
int
server_sendf(const char *fmt, ...)
{
	char buf[2048];

	// Tell the event loop we want to write out
	ev_set_writeout(ircfd, 1);

	// Chuck stuff onto the buffer
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	debugf("server >> %s", buf);

	n += snprintf(buf+n, sizeof(buf)-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.

	return bufio_write(&server_bufio, buf, n);
}

/* server_sendmsg sends an IRC message to the server.
 *
 * The number of bytes written to the send buffer is returned, or -1 upon
 * failure.
 */
int
server_sendmsg(struct irc_message *msg)
{
	char buf[2048];
	int n;

	if ((n = irc_string(msg, buf, sizeof(buf))) == -1)
		return -1;

	// Tell the event loop we want to write out
	ev_set_writeout(ircfd, 1);

	// Chuck stuff onto the buffer
	debugf("server >> %s", buf);

	n += snprintf(buf+n, sizeof(buf)-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.

	return bufio_write(&server_bufio, buf, n);
}

int
server_readable(void)
{
	int n;

	if ((n = bufio_readable(&server_bufio, ircfd)) == -1) {
		warnf("failed reading from server: %s", strerror(errno));
		close(ircfd);
		return 0;
	}

	if (!n) // Partial read
		return 0;

	debugf("server << %s", server_bufio.recvbuf);

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(server_bufio.recvbuf, &msg)) {
		warnf("Failed to parse IRC message from server. Disconnecting.");
		close(ircfd);
		return 0;
	}

	if (strcmp(msg.command, "ERROR") == 0) {
		errorf("Server error: %s", msg.params[0]); 
		close(ircfd);

		// Pass it onto everyone.
		server_client_forward(&msg);
		return 0;
	} else if (strcmp(msg.command, "PING") == 0) {
		// Return a PONG
		msg.command = "PONG";
		msg.source = NULL;
		msg.tags = NULL;

		server_sendmsg(&msg);
		return 1;
	}

	// Pass it onto everyone.
	server_client_forward(&msg);

	return 1;
}

void
server_writable(void)
{
	int n = bufio_writable(&server_bufio, ircfd);

	if (n >= 1) { // No more data
		// Tell the event loop we no longer want to write out
		ev_set_writeout(ircfd, 1);
	} else if (n == -1) {
		warnf("Server write failed: %s", strerror(errno));
		close(ircfd);
	}
}
