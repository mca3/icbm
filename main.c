#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "irc.h"
#include "client.h"
#include "server.h"

static int pollfdsz = 32;
static int pollfdptr = 1; // First are is server and IRC network
static struct pollfd *pollfds;

int acceptfd = -1;
int ircfd = -1;

/* listenfd attempts to listen on addr:port and exits if it cannot. */
static int
listenfd(char *addr, char *port)
{
	// Kindly pilfered from Beej's networking guide

	int sockfd;
	struct addrinfo hints = {0}, *servinfo, *p;
	int yes = 1;
	int rv;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		errorf("getaddrinfo: %s", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			errorf("setsockopt: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			warnf("bind: %s", strerror(errno));
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL) {
		errorf("Couldn't bind anywhere. Exiting.");
		exit(EXIT_FAILURE);
	}

	if (listen(sockfd, 10) == -1) {
		errorf("listen: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK); // Set non-blocking
	return sockfd;
}

static int
connectfd(char *addr, char *port)
{
	int sockfd, numbytes;  
	struct addrinfo hints = {0}, *servinfo, *p;
	int rv;

	assert(addr != NULL);

	port = port ? port : "6667";

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		errorf("getaddrinfo: %s", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			warnf("connect: %s", strerror(errno));
			continue;
		}

		break;
	}

	if (p == NULL) {
		errorf("Couldn't connect to IRC server");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);
	
	fcntl(sockfd, F_SETFL, O_NONBLOCK); // Set non-blocking
	return sockfd;
}

static void
accept_client()
{
	int fd = accept(acceptfd, NULL, NULL);
	if (fd == -1) return;

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
}

static void
pollloop(void)
{
	int i;

	clients = realloc(NULL, sizeof(struct client)*clientsz);
	pollfds = realloc(NULL, sizeof(struct pollfd)*pollfdsz);

	// Set the first two pollfds up
	pollfds[0].fd = acceptfd;
	pollfds[0].events = POLLIN;

	pollfds[1].fd = ircfd;
	pollfds[1].events = POLLIN;

	for (;;) {
		i = poll(pollfds, pollfdptr+1, -1);
		if (i == -1) {
			errorf("poll: %s", strerror(errno));
			return;
		}

		if (pollfds[0].revents & POLLIN) {
			accept_client();
			continue;
		}

		if (pollfds[1].revents & POLLIN)
			server_readable();
		if (pollfds[1].revents & POLLOUT)
			server_writable();
		if (pollfds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
			errorf("Server connection died!");
			return;
		}

		for (int i = 2; i <= pollfdptr; ++i) {
			if (pollfds[i].revents & POLLIN)
				client_readable(pollfds[i].fd);
			if (pollfds[i].revents & POLLOUT)
				client_writable(pollfds[i].fd);
		}

		// Remove dead clients
		for (int i = 2; i <= pollfdptr; ++i) {
			if (!(pollfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)))
				continue;

			// Find index in clients
			int cli;
			for (cli = 0; cli < clientptr; ++cli) {
				if (clients[cli].fd == pollfds[i].fd)
					break;
			}
			
			debugf("Connection on fd %d died", pollfds[i].fd);

			// Delete from clients
			// We must move all clients ahead of it back one space
			memmove(&clients[cli], &clients[cli+1], sizeof(struct client)*(clientptr-cli));
			clientptr--;

			// Also delete from pollfds
			memmove(&pollfds[i], &pollfds[i+1], sizeof(struct pollfd)*(pollfdptr-i));
			pollfdptr--;
		}
	}
}

int
main(void)
{
	acceptfd = listenfd("127.0.0.1", "16667");
	ircfd = connectfd("127.0.0.1", "6667");
	debugf("listen fd %d, irc fd %d", acceptfd, ircfd);
	pollloop();
	close(acceptfd);
	close(ircfd);
}
