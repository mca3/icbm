#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "bufio.h"
#include "irc.h"
#include "log.h"
#include "server.h"

static struct bufio server_bufio = {0};

void
server_readable(void)
{
	int n;

	if ((n = bufio_readable(&server_bufio, ircfd)) == -1) {
		warnf("failed reading from server: %s", strerror(errno));
		close(ircfd);
		return;
	}

	if (!n) // Partial read
		return;

	debugf("server << %s", server_bufio.recvbuf);

	// Parse message
	struct irc_message msg = {0};

	if (irc_parse(server_bufio.recvbuf, &msg)) {
		warnf("Failed to parse IRC message from server. Disconnecting.");
		close(ircfd);
		return;
	}

	debugf("server RX command: %s", msg.command);

	if (strcmp(msg.command, "ERROR") == 0) {
		errorf("Server error: %s", msg.params[0]); 
		close(ircfd);

		// Pass it onto everyone.
		// client_forward(&msg);
		return;
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
}

void
server_writable(void)
{

}
