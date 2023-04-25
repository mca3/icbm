/* This file is a part of libmca, where code you would've written anyway lives.
 *   https://github.com/mca3/libmca * https://int21h.xyz/projects/libmca.html
 *
 * Copyright (c) 2023 mca
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include "ev.h"

static int
ensure(struct mca_ev *ev, size_t n)
{
	size_t new_size = ev->len + n;

	if (new_size > ev->cap) {
		struct pollfd *pfds;

		if (!(pfds = realloc(ev->pfds, sizeof(*pfds) * new_size)))
			return -1;

		ev->pfds = pfds;
		ev->cap = new_size;
	}

	return 1;
}

static size_t
find(struct mca_ev *ev, int fd, struct pollfd **out)
{
	struct pollfd *pfd;
	size_t i;

	for (i = 0; i < ev->len; ++i) {
		pfd = &ev->pfds[i];

		if (pfd->fd == fd)
			break;
	}

	if (!pfd || pfd->fd != fd)
		return -1;

	if (out)
		*out = pfd;

	return i;
}

static int
pending_writes(struct mca_ev *ev)
{
	size_t i;

	if (!ev->on_writable)
		return 0;

	for (i = 0; i < ev->len; ++i)
		if (ev->pfds[i].events & POLLOUT)
			return 1;

	return 0;
}

static int
do_poll(struct mca_ev *ev, int timeout, int ignore_read)
{
	int i;

	i = poll(ev->pfds, ev->len, timeout);
	if (i == -1)
		return i;

	for (i = 0; i < ev->len; ++i) {
		if (!ignore_read && ev->pfds[i].revents & POLLIN)
			while (ev->on_readable(ev, ev->pfds[i].fd, ev->userdata));

		if (ev->pfds[i].revents & POLLOUT && ev->on_writable)
			ev->on_writable(ev, ev->pfds[i].fd, ev->userdata);
	}

	// Remove dead clients
	for (i = 0; i < ev->len; ++i) {
		if (!(ev->pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)))
			continue;
		
		// TODO: don't use this; this searches through the list again
		mca_ev_remove(ev, ev->pfds[i].fd);
	}

	return 0;
}

/* Creates a new instance of ev and stores it in its argument.
 *
 * If allocation fails, -1 is returned and its argument is left unmodified.
 */
int
mca_ev_new(struct mca_ev **ev)
{
	struct mca_ev *nev;

	if (!(nev = malloc(sizeof(*nev))))
		return -1;
	memset(nev, 0, sizeof(*nev));

	// Allocate the pfd array.
	if (ensure(nev, MCA_EV_INIT_SIZE) == -1) {
		free(nev);
		return -1;
	}

	*ev = nev;

	return 0;
}

/* Frees ev. */
void
mca_ev_free(struct mca_ev *ev)
{
	if (ev->pfds)
		free(ev->pfds);
	free(ev);
}

/* Appends a file descriptor to the event loop using flags.
 *
 * If flags is not zero, the following constants have effect:
 * - MCA_EV_READ: this file descriptor triggers on_readable.
 * - MCA_EV_WRITE: this file descriptor triggers on_writable.
 *
 * The flags may be set at any time.
 *
 * On error, -1 is returned.
 */
int
mca_ev_append(struct mca_ev *ev, int fd, int flags)
{
	struct pollfd *pfd;

	if (ensure(ev, 1) == -1)
		return -1;

	pfd = &ev->pfds[ev->len++];

	pfd->fd = fd;
	pfd->events = 0;
	pfd->revents = 0;

	if (flags & MCA_EV_READ)
		pfd->events |= POLLIN;
	if (flags & MCA_EV_WRITE)
		pfd->events |= POLLOUT;

	return 0;
}

/* Manually remove a file descriptor.
 *
 * Normally, ev will automatically remove file descriptors that are considered
 * "dead," i.e. poll(2) says that the file descriptor has entered an error
 * state.
 *
 * Note that calling this guarantees that you are going to miss events for at
 * least one file descriptor as in normal usage this method is only called
 * during readable and writable events, and the handlers for those events are
 * called from an iteration over all its known file descriptors.
 *
 * This still calls on_remove.
 */
void
mca_ev_remove(struct mca_ev *ev, int fd)
{
	struct pollfd *pfd = NULL;
	size_t i;

	if ((i = find(ev, fd, &pfd)) == -1)
		return;

	if (ev->on_remove)
		ev->on_remove(ev, fd, ev->userdata);

	ev->len--;
	memmove(ev->pfds + i, ev->pfds + i + 1, (ev->len - i)*sizeof(*pfd));
}

/* Sets flags on a file descriptor.
 *
 * A val of 0 disables all flags specified via NAND.
 * A val of 1 enables all flags specified via OR.
 */
void
mca_ev_set_flags(struct mca_ev *ev, int fd, int flags, int val)
{
	struct pollfd *pfd = NULL;
	size_t i;
	int mask = 0;

	if ((i = find(ev, fd, &pfd)) == -1)
		return;

	if (flags & MCA_EV_READ)
		mask |= POLLIN;
	if (flags & MCA_EV_WRITE)
		mask |= POLLOUT;

	if (val)
		pfd->events |= mask;
	else
		pfd->events &= ~mask;
}

/* Polls until all pending writes have been completed.
 *
 * There are pending writes when a file descriptor is marked as requesting
 * MCA_EV_WRITE.
 * No reads will be fulfilled, but they still will be polled for; a call to
 * here can be computationally expensive as it is stuck spinning due to
 * outstanding reads.
 *
 * See mca_ev_poll for a description on the return codes.
 */
int
mca_ev_flush(struct mca_ev *ev, int timeout)
{
	int i;

	while (pending_writes(ev)) {
		if ((i = do_poll(ev, timeout, 1)) == -1)
			return i;
	}

	return 0;
}

/* Waits for something to happen and acts on it.
 *
 * On error, -1 is returned.
 * The error may or may not be fatal, such a EINTR.
 */
int
mca_ev_poll(struct mca_ev *ev, int timeout)
{
	return do_poll(ev, timeout, 0);
}
