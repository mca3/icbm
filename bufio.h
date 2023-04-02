#include <stddef.h>

struct bufio {
	char sendbuf[4096];
	char recvbuf[4096];
	int sendptr;
	int recvptr, last_recvptr;
};

int bufio_readable(struct bufio *b, int fd);
int bufio_writable(struct bufio *b, int fd);
int bufio_write(struct bufio *b, void *data, size_t n);
