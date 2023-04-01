#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "irc.h"
#include "log.h"
#include "server.h"

static char server_sendbuf[4096];
static char server_recvbuf[4096];
static int server_sendptr = 0;
static int server_recvptr = 0;

int
server_ircf(const char *fmt, ...)
{

}

void
server_readable(void)
{
	// Read as much as we can
	int n = read(ircfd, server_recvbuf+server_recvptr, sizeof(server_recvbuf)-server_recvptr);
	if (n == -1) {
		// Hangup!
		warnf("failed reading from server: %s", strerror(errno));
		close(ircfd);
		return;
	}

	server_recvptr += n;

	// Try to find a message
	char *nl = memchr(server_recvbuf, '\n', server_recvptr);
	if (nl == NULL) // Nothing yet
		return;

	// Set zeroes
	if (nl-(server_recvbuf) > 1 && *(nl - 1) == '\r')
		*(nl - 1) = 0;
	else
		*(nl) = 0;

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(server_recvbuf, &msg)) {
		warnf("Failed to parse IRC message from server. Disconnecting.");
		close(ircfd);
		return;
	}

	debugf("server RX command: %s", msg.command);

	if (strcmp(msg.command, "ERROR") == 0) {
		errorf("Server error: %s", msg.params[0]); 
		close(ircfd);
		return;
	}

	// Reset buffer
	memmove(server_recvbuf, nl+1, server_recvptr - (nl - server_recvbuf));
	server_recvptr -= (nl - server_recvbuf)+1;
}

void
server_writable(void)
{

}
