#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "client.h"
#include "evloop.h"
#include "irc.h"
#include "log.h"
#include "server.h"

static int pollfdsz = 32;
static int pollfdptr = 1; // First are is server and IRC network
static struct pollfd *pollfds;

int acceptfd = -1;

static void
accept_client()
{
	int fd = accept(acceptfd, NULL, NULL);
	if (fd == -1) return;

	fcntl(fd, F_SETFL, O_NONBLOCK); // Set non-blocking

	if (clientptr + 1 >= clientsz) {
		clientsz *= 2;
		clients = realloc(clients, sizeof(struct client)*clientsz);
	}

	if (pollfdptr + 1 >= pollfdsz) {
		pollfdsz *= 2;
		pollfds = realloc(pollfds, sizeof(struct pollfd)*pollfdsz);
	}

	++clientptr;
	++pollfdptr;

	memset(&clients[clientptr], 0, sizeof(struct client));
	clients[clientptr].fd = fd;

	pollfds[pollfdptr].fd = fd;
	pollfds[pollfdptr].events = POLLIN;

	debugf("New connection on fd %d. pollfdptr is %d", fd, pollfdptr);

	client_sendf(&clients[clientptr], "PING :%d", time(NULL));
}

static void
remove_client(int fd, int pfd)
{
	// Find index in clients
	int cli;
	for (cli = 0; cli < clientptr; ++cli) {
		if (clients[cli].fd == pollfds[pfd].fd)
			break;
	}

	// Delete from clients
	// We must move all clients ahead of it back one space
	memmove(&clients[cli], &clients[cli+1], sizeof(struct client)*(clientptr-cli));
	clientptr--;

	// Also delete from pollfds
	memmove(&pollfds[pfd], &pollfds[pfd+1], sizeof(struct pollfd)*(pollfdptr-pfd));
	pollfdptr--;
}

void
ev_set_writeout(int fd, int val)
{
	for (int i = 0; i <= pollfdptr; ++i) {
		if (pollfds[i].fd == fd)
			pollfds[i].events = val ? (pollfds[i].events | POLLOUT) : (pollfds[i].events ^ POLLOUT);
	}
}

void
init_evloop(void)
{
	clients = realloc(NULL, sizeof(struct client)*clientsz);
	if (!clients) {
		errorf("Failed to allocate clients (%d bytes)", sizeof(struct client)*clientsz);
		exit(EXIT_FAILURE);
	}

	pollfds = realloc(NULL, sizeof(struct pollfd)*pollfdsz);
	if (!pollfds) {
		errorf("Failed to allocate pollfds (%d bytes)", sizeof(struct pollfd)*pollfdsz);
		exit(EXIT_FAILURE);
	}

	// Set the first two pollfds up
	pollfds[0].fd = acceptfd;
	pollfds[0].events = POLLIN;

	pollfds[1].fd = ircfd;
	pollfds[1].events = POLLIN;
}

void
evloop(void)
{
	int i;

	for (;;) {
		i = poll(pollfds, pollfdptr+1, -1);
		if (i == -1) {
			errorf("poll: %s", strerror(errno));
			return;
		}

		if (pollfds[0].revents & POLLIN) {
			accept_client();
			continue; // Modifies pollfds, safe to just restart
		}

		if (pollfds[1].revents & POLLIN)
			while (server_readable());
		if (pollfds[1].revents & POLLOUT)
			server_writable();
		if (pollfds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
			errorf("Server connection died!");
			return;
		}

		for (int i = 2; i <= pollfdptr; ++i) {
			if (pollfds[i].revents & POLLIN)
				while (client_readable(pollfds[i].fd));
			if (pollfds[i].revents & POLLOUT)
				client_writable(pollfds[i].fd);
		}

		// Remove dead clients
		for (int i = 2; i <= pollfdptr; ++i) {
			if (!(pollfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)))
				continue;
			
			debugf("Connection on fd %d died", pollfds[i].fd);
			remove_client(pollfds[i].fd, i);
		}
	}
}
