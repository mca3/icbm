#define _XOPEN_SOURCE 500

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "client.h"
#include "evloop.h"
#include "log.h"
#include "server.h"
#include "vec.h"

static int cli_cap(struct client *c, struct irc_message *msg);
static int cli_login(struct client *c, struct irc_message *msg);
static int cli_ping(struct client *c, struct irc_message *msg);

static struct {
	char *command;
	int (*f)(struct client *c, struct irc_message *msg);
} client_dispatch[] = {
	{ "CAP",	cli_cap },
	{ "USER",	cli_login },
	{ "NICK",	cli_login },

	{ "PING",	cli_ping },
	{ "PONG",	cli_ping },
};

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

/* client_sendmsg sends an IRC message to the client.
 *
 * The number of bytes written to the send buffer is returned, or -1 upon
 * failure.
 */
int
client_sendmsg(struct client *c, struct irc_message *msg)
{
	char buf[2048];
	int n;

	if ((n = irc_string(msg, buf, sizeof(buf))) == -1)
		return -1;

	// Tell the event loop we want to write out
	ev_set_writeout(c->fd, 1);

	// Chuck stuff onto the buffer
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

	// Try to hit a recognized command.
	for (size_t i = 0; i < sizeof(client_dispatch)/sizeof(*client_dispatch); ++i)
		if (strcmp(client_dispatch[i].command, msg.command) == 0)
			return client_dispatch[i].f(c, &msg);

	// Pass onto server if all else fails
	server_sendmsg(&msg);

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

/*
 * The rest of the file handles commands.
 */

int
cli_cap(struct client *c, struct irc_message *msg)
{

}

int
cli_login(struct client *c, struct irc_message *msg)
{
	if (c->nick)
		free(c->nick);
	c->nick = strdup(msg->params[0]);

	client_sendf(c, ":%s 001 %s :Welcome to icbm, %s", "example.com", c->nick, c->nick);

	struct irc_message out = {0};
	out.source = "example.com"; // TODO
	out.command = "005";
	out.params[0] = c->nick; // client ident

	// TODO: Ensure 512 bytes is not exceeded

	size_t ctr = 1;
	for (size_t i = 0; i < server_isupport.len; ++i) {
		out.params[ctr++] = server_isupport.data[i];

		if (ctr == IRC_PARAM_MAX-2) {
			out.params[ctr] = "are supported by this server";
			client_sendmsg(c, &out);
			ctr = 1;
		}
	}

	if (ctr > 1) {
		out.params[ctr] = "are supported by this server";
		if (ctr+1 < IRC_PARAM_MAX)
			out.params[ctr+1] = NULL;
		client_sendmsg(c, &out);
	}

	return 1;
}

int
cli_ping(struct client *c, struct irc_message *msg)
{
	if (strcmp(msg->command, "PONG") == 0)
		return 1;

	msg->tags = NULL;
	msg->source = "example.com"; // TODO
	
	client_sendmsg(c, msg);
	return 1;
}
