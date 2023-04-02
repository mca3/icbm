#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>

#include "bufio.h"
#include "irc.h"
#include "log.h"
#include "server.h"
#include "main.h"

static struct bufio server_bufio = {0};

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

	// Find our pfd
	for (int i = 0; i <= pollfdptr; ++i) {
		if (pollfds[i].fd == ircfd) {
			// Add POLLOUT
			pollfds[i].events |= POLLOUT;
			break;
		}
	}

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
		// client_forward(&msg);
		return 0;
	} else if (strcmp(msg.command, "PING") == 0) {
		/*
		// Return a PONG
		msg.command = "PONG";
		msg.source = NULL;
		msg.tags = NULL;
		server_send(&msg);
		return;
		*/
	}

	// Pass it onto everyone.
	// client_forward(&msg);
	return 1;
}

void
server_writable(void)
{
	int n = bufio_writable(&server_bufio, ircfd);

	if (n >= 1) { // No more data
		// Remove POLLOUT if we have nothing more to send
		for (int i = 0; i <= pollfdptr; ++i) {
			if (pollfds[i].fd == ircfd) {
				pollfds[i].events ^= POLLOUT;
				pollfds[i].revents ^= POLLOUT;
				break;
			}
		}
	} else if (n == -1) {
		warnf("Server write failed: %s", strerror(errno));
		close(ircfd);
	}
}
