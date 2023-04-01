struct client {
	int fd;
	char sendbuf[2048];
	char recvbuf[2048];
};

void client_readable(int fd);
void client_writable(int fd);
