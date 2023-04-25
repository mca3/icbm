#define _XOPEN_SOURCE 500

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bufio.h"
#include "client.h"
#include "ev.h"
#include "irc.h"
#include "log.h"
#include "main.h"
#include "server.h"
#include "vec.h"

static struct bufio server_bufio = {0};

// Initialized by main
struct mca_vector server_isupport = {0};

static int srv_error(struct irc_message *msg);
static int srv_isupport(struct irc_message *msg);
static int srv_ping(struct irc_message *msg);

static struct {
	char *command;
	int (*f)(struct irc_message *msg);
} server_dispatch[] = {
	{ "005",	srv_isupport },

	{ "ERROR",	srv_error },
	{ "PING",	srv_ping },
	{ "PONG",	srv_ping },
};

static void
server_client_forward(struct irc_message *msg)
{
	// Write the message to a buffer first
	char buf[2048];
	int n;

	if ((n = irc_string(msg, buf, sizeof(buf))) == -1)
		return;

	// Tell the event loop we want to write out
	mca_ev_set_write(ev, ircfd, 1);

	// Chuck stuff onto the buffer
	debugf("* >> %s", buf);

	n += snprintf(buf+n, sizeof(buf)-n, "\r\n");

	// TODO: Handle overfull scenarios gracefully. *printf ALWAYS returns
	// what it would have written.

	// Send to all clients
	// TODO: Only those whom are authenticated
	for (int i = 0; i < clientptr; ++i) {
		// TODO: This should be a client_... function.
		mca_ev_set_write(ev, clients[i].fd, 1);
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
	mca_ev_set_write(ev, ircfd, 1);

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
	mca_ev_set_write(ev, ircfd, 1);

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

	// Try to hit a recognized command.
	for (size_t i = 0; i < sizeof(server_dispatch)/sizeof(*server_dispatch); ++i)
		if (strcmp(server_dispatch[i].command, msg.command) == 0)
			return server_dispatch[i].f(&msg);

	// Fallthrough case: pass it onto everyone.
	server_client_forward(&msg);

	return 1;
}

void
server_writable(void)
{
	int n = bufio_writable(&server_bufio, ircfd);

	if (n > 0) { // No more data
		// Tell the event loop we no longer want to write out
		mca_ev_set_write(ev, ircfd, 0);
	} else if (n == -1) {
		warnf("Server write failed: %s", strerror(errno));
		close(ircfd);
	}
}

/* gets the index of the isupport key if there is one, otherwise returns -1. */
static size_t
isupport_key(char *key)
{
	for (size_t i = 0; i < server_isupport.len; ++i)
		if (strcmp(server_isupport.data[i], key) == 0)
			return i;
	return -1;
}

/*
 * The following section is all command related
 */

int
srv_error(struct irc_message *msg)
{
	errorf("Server error: %s", msg->params[0]); 
	close(ircfd);

	// Pass it onto everyone.
	server_client_forward(msg);
	return 0;
}

int
srv_isupport(struct irc_message *msg)
{
	for (size_t i = 1; i < IRC_PARAM_MAX; ++i) {
		if (!msg->params[i] || strchr(msg->params[i], ' '))
			break;

		char *v = strdup(msg->params[i]);

		// Mangles this paramater, but leaves us the key
		char *eq = strchr(v, '=');
		if (eq)
			*eq = 0;

		size_t j = isupport_key(msg->params[i]);

		// Restore so we can forward this message
		strcpy(v, msg->params[i]);
		if (j != -1) {
			free(server_isupport.data[j]);
			server_isupport.data[j] = v;
		} else
			mca_vector_push(&server_isupport, v);
	}

	// Pass it onto everyone.
	server_client_forward(msg);

	return 1;
}

int
srv_ping(struct irc_message *msg)
{
	if (strcmp(msg->command, "PONG") == 0)
		return 1;

	msg->command = "PONG";
	msg->source = NULL;
	msg->tags = NULL;

	server_sendmsg(msg);
	return 1;
}
