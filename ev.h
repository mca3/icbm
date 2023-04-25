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
