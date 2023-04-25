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
#include <time.h>

#include "client.h"
#include "ev.h"
#include "irc.h"
#include "log.h"
#include "main.h"
#include "server.h"

int ircfd = -1;
int acceptfd = -1;

struct mca_ev *ev;

static char *username = NULL;
static char *nickname = NULL;

static int running = 1;

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

/* connectfd connects to addr:port over TCP. */
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

	freeaddrinfo(servinfo);

	if (p == NULL)
		return -1;
	
	fcntl(sockfd, F_SETFL, O_NONBLOCK); // Set non-blocking
	return sockfd;
}

static void
accept_conn(void)
{
	int fd = accept(acceptfd, NULL, NULL);
	if (fd == -1) return;

	fcntl(fd, F_SETFL, O_NONBLOCK); // Set non-blocking

	if (clientptr + 1 >= clientsz) {
		clientsz *= 2;
		clients = realloc(clients, sizeof(struct client)*clientsz);
	}

	memset(&clients[clientptr], 0, sizeof(struct client));
	clients[clientptr].fd = fd;

	mca_ev_append(ev, fd, MCA_EV_READ);

	debugf("New connection on fd %d", fd);

	client_sendf(&clients[clientptr], "PING :%d", time(NULL));

	++clientptr;
}

static int
evremove(struct mca_ev *, int fd, void *)
{
	if (fd == ircfd || fd == acceptfd) {
		if (fd == ircfd)
			errorf("Server connection closed");
		else
			errorf("Listening socket closed");
		running = 0;

		return 0;
	}

	debugf("Connection on fd %d died", fd);
			
	// Find index in clients
	int cli;
	for (cli = 0; cli < clientptr; ++cli) {
		if (clients[cli].fd == fd)
			break;
	}

	// Free nick
	if (clients[cli].nick)
		free(clients[cli].nick);

	// We must move all clients ahead of it back one space
	memmove(&clients[cli], &clients[cli+1], sizeof(struct client)*(clientptr-cli));
	clientptr--;
}

static int
evread(struct mca_ev *, int fd, void *)
{
	if (fd == acceptfd) {
		accept_conn();
		return 0;
	} else if (fd == ircfd)
		return server_readable();

	return client_readable(fd);
}

static int
evwrite(struct mca_ev *, int fd, void *)
{
	if (fd == ircfd) {
		server_writable();
		return 0;
	}

	client_writable(fd);
	return 0;
}

static void
evloop(void)
{
	int i;

	while (running) {
		i = mca_ev_poll(ev, -1);
		if (i == -1) {
			errorf("poll: %s", strerror(errno));
			break;
		}
	}

	mca_ev_flush(ev, -1);
}

int
main(int argc, char *argv[])
{
	int opt;

	char *address = "127.0.0.1";
	char *port = "6667";
	char *laddress = "127.0.0.1";
	char *lport = "16667";

	while ((opt = getopt(argc, argv, "u:n:a:p:A:P:")) != -1) {
		switch (opt) {
		case 'u': username = optarg; break;
		case 'n': nickname = optarg; break;
		case 'a': address = optarg; break;
		case 'p': port = optarg; break;
		case 'A': laddress = optarg; break;
		case 'P': lport = optarg; break;
		}
	}

	if (!username) {
		if (!(username = getenv("LOGNAME"))) {
			errorf("Unable to get username. LOGNAME was not set.");
			exit(EXIT_FAILURE);
		}
	}

	if (!nickname)
		nickname = username;

	// Setup event loop
	if (mca_ev_new(&ev) == -1) {
		errorf("Failed to setup event loop.");
		exit(EXIT_FAILURE);
	}
	ev->on_readable = evread;
	ev->on_writable = evwrite;
	ev->on_remove = evremove;

	// Initialize client list
	clients = realloc(NULL, sizeof(struct client)*clientsz);
	if (!clients) {
		errorf("Failed to allocate clients", sizeof(struct client)*clientsz);
		exit(EXIT_FAILURE);
	}

	// Connect
	if ((ircfd = connectfd(address, port)) == -1) {
		errorf("Failed to connect to the IRC server.");
		mca_ev_free(ev);
		free(clients);
		exit(EXIT_FAILURE);
	}

	if ((acceptfd = listenfd(laddress, lport)) == -1) {
		errorf("Failed to listen.");
		mca_ev_free(ev);
		close(ircfd);
		free(clients);
		exit(EXIT_FAILURE);
	}

	debugf("listen fd %d, irc fd %d", acceptfd, ircfd);

	// Further setup event loop.
	mca_ev_append(ev, ircfd, MCA_EV_READ);
	mca_ev_append(ev, acceptfd, MCA_EV_READ);

	server_sendf("NICK :%s", nickname);
	server_sendf("USER %s 0 * :%s", nickname, "icbm");

	// Jump into the event loop.
	evloop();

	// Cleanup.
	close(acceptfd);
	close(ircfd);

	mca_ev_free(ev);

	size_t cli;
	for (cli = 0; cli < clientptr; ++cli) {
		if (clients[cli].nick)
			free(clients[cli].nick);
		close(clients[cli].fd);
	}
	free(clients);

	for (size_t i = 0; i < server_isupport.len; ++i)
		free(server_isupport.data[i]);
	free(server_isupport.data);
}
