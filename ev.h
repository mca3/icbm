/* This file is a part of libmca, where code you would've written anyway lives.
 *   https://github.com/mca3/libmca * https://int21h.xyz/projects/libmca.html
 *
 * Copyright (c) 2023 mca
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with  or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBMCA_EV_H
#define LIBMCA_EV_H

#include <poll.h>

#define MCA_EV_READ (1 << 0)
#define MCA_EV_WRITE (1 << 1)

#ifndef MCA_EV_INIT_SIZE
#define MCA_EV_INIT_SIZE 32
#endif

struct mca_ev {
	struct pollfd *pfds;
	size_t len;
	size_t cap;

	void *userdata;

	int safe;

	int (*on_readable)(struct mca_ev *ev, int fd, void *userdata);
	int (*on_writable)(struct mca_ev *ev, int fd, void *userdata);
	int (*on_remove)(struct mca_ev *ev, int fd, void *userdata);
};

#define mca_ev_set_userdata(ev, data) do { \
	ev->userdata = data; \
} while (0)

#define mca_ev_set_write(ev, fd, val) mca_ev_set_flags(ev, fd, MCA_EV_WRITE, val)

int mca_ev_new(struct mca_ev **ev);
void mca_ev_free(struct mca_ev *ev);

int mca_ev_append(struct mca_ev *ev, int fd, int flags);
void mca_ev_remove(struct mca_ev *ev, int fd);
void mca_ev_set_flags(struct mca_ev *ev, int fd, int flags, int val);

int mca_ev_flush(struct mca_ev *ev, int timeout);
int mca_ev_poll(struct mca_ev *ev, int timeout);

#endif /* LIBMCA_EV_H */
