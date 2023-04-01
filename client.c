#include <unistd.h>

#include "client.h"
#include "log.h"

void
client_readable(int fd)
{
	debugf("Read fd %d", fd);
	close(fd);
}

void
client_writable(int fd)
{

}
