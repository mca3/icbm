#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bufio.h"
#include "log.h"

static char *
scan_newline(struct bufio *b)
{
	// Try to find a message delimiter.
	// We're looking just for a newline because that is guaranteed to be
	// there in an IRC message.
	char *nl = memchr(b->recvbuf, '\n', b->recvptr);
	if (nl == NULL) // Nothing yet
		return NULL;

	// Set zeroes on the delimiters.
	// Remove '\r' if it is there, because we are liberal in what we accept
	// and '\r' can be omitted.
	if (nl-(b->recvbuf) > 1 && *(nl - 1) == '\r')
		*(nl - 1) = 0;
	else
		*(nl) = 0;

	// This field indicates that this method found some data, and that
	// everything up to last_recvptr should be removed.
	// Reset by the next call to bufio_readable.
	b->last_recvptr = nl - b->recvbuf + 1;
	return nl;
}

/* bufio_readable attempts to read a newline-delimited message from fd.
 * The newline will be mangled into a null byte, and if the newline is
 * preceeded by a '\r' then that will also be mangled into a null byte.
 *
 * If it is unable to but nothing fatal occurs, 0 is returned to indicate that
 * there is no new data to read.
 *
 * If something fatal happens, -1 is returned and errno is set. The bufio
 * object may no longer be used and the underlying fd should be closed.
 *
 * If it is successful, the number of bytes read is returned and the data may
 * be accessed at b->recvbuf.
 */
int
bufio_readable(struct bufio *b, int fd)
{
	int n;
	char *msg;
	assert(b != NULL);

	// IRC messages are supposed to be at most 2048 characters in length
	// (technically 512 going by the RFC) so provided recvbuf is still 4096
	// we can store up to 8 RFC length messages at full length, or 2 full
	// length "V3" messages.
	//
	// If we fill recvbuf and don't get a message out of it, it is safe to
	// say that we will likely never get any messages out of it, ever. Set
	// errno and fail.
	if (b->recvptr >= sizeof(b->recvbuf)) {
		errno = ERANGE;
		return -1;
	}

	// We do not reset recvbuf at the end of this function, but rather at
	// the beginning because it allows processing to continue normally
	// without a need to call something like bufio_done_read().
	if (b->last_recvptr) {
		memmove(b->recvbuf, b->recvbuf+b->last_recvptr, b->recvptr - b->last_recvptr);
		b->recvptr -= b->last_recvptr;
		b->last_recvptr = 0;
	}

	/* This region kinda sucks but I don't know how else to do it. I'll
	 * explain:
	 *
	 * - If there's a message in the buffer, we return >= 1 to say there is.
	 *   The message gets invalidated by the next call to bufio_readable,
	 *   and if you're doing it right, then a call to a function that does
	 *   stuff with the data from bufio will loop until the return code is
	 *   0, as in no data.
	 * - If not, we attempt to read data.
	 * - If the read fails with EAGAIN, return 0 and stop. There is nothing
	 *   to read currently.
	 * - If the read succeeds, restart the cycle.
	 */

read:
	// Determine if there's any data we can use already in the buffer.
	if ((msg = scan_newline(b)) != NULL) {
		// Yes, there is! Return something for success.
		return b->last_recvptr;
	}

	// Read as much as we can and loop back up above if errno isn't EAGAIN.
	n = read(fd, b->recvbuf+b->recvptr, sizeof(b->recvbuf)-b->recvptr);
	if (n == -1) {
		if (errno & EAGAIN) return 0;
		return n;
	} else if (n == 0) {
		// File descriptor likely closed.
		return -1;
	}

	b->recvptr += n;

	goto read;

	/* Unreachable. */
}

/* bufio_writable attempts to write outstanding data to fd.
 *
 * If a partial write occurs, 0 is returned. POLLOUT should remain an event
 * flag if using poll(2).
 *
 * If a failed write occurs, -1 is returned. The bufio struct should no longer
 * be used and the underlying fd should be closed.
 *
 * If a write succeeds and there is no more outstanding data, 1 is returned.
 * POLLOUT should be removed from the event flags if using poll(2).
 */
int
bufio_writable(struct bufio *b, int fd)
{
	assert(b != NULL);

	int n = write(fd, b->sendbuf, b->sendptr);
	if (n == -1)
		return -1;

	// Move everything back.
	//
	// Unlike bufio_readable where the data should be valid after a call
	// but the previous data invalidated by a call, we need not worry about
	// when data is valid because the users of bufio aren't supposed to
	// care about it once they send it.
	memmove(b->sendbuf, b->sendbuf+b->sendptr, b->sendptr-n);
	b->sendptr -= n;

	// 1 if true, 0 if false.
	return b->sendptr == 0;
}

/* bufio_write writes data to the send buffer, which will eventually be sent
 * when bufio_writable is called.
 *
 * If an error occurs, -1 is returned and errno is set. This will only happen
 * when you try to send data faster than the client can receive it.
 *
 * Otherwise, the number of bytes written to the send buffer is returned. Users
 * of poll(2) should set POLLOUT.
 */
int
bufio_write(struct bufio *b, void *data, size_t n)
{
	assert(b != NULL);

	// Ensure data can fit.
	if (b->sendptr + n + 1 > sizeof(b->sendbuf))
		return -1;

	// Copy.
	memmove(b->sendbuf+b->sendptr, data, n);
	b->sendptr += n;

	return n;
}
