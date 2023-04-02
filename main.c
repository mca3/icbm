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

#include "log.h"
#include "irc.h"
#include "client.h"
#include "server.h"
#include "evloop.h"

int ircfd = -1;

static char *username = NULL;
static char *nickname = NULL;

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

	acceptfd = listenfd("127.0.0.1", "16667");
	ircfd = connectfd("127.0.0.1", "6667");
	debugf("listen fd %d, irc fd %d", acceptfd, ircfd);

	init_evloop();

	server_sendf("NICK :%s", nickname);
	server_sendf("USER %s 0 * :%s", nickname, "icbm");

	evloop();

	close(acceptfd);
	close(ircfd);
}
