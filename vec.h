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

#ifndef LIBMCA_VEC_H
#define LIBMCA_VEC_H

#include <stddef.h>

struct mca_vector {
	void **data;
	size_t len;
	size_t cap;
};

struct mca_vector *mca_vector_new(size_t cap);
void mca_vector_free(struct mca_vector *v);

int mca_vector_ensure(struct mca_vector *v, size_t size);
size_t mca_vector_push(struct mca_vector *v, void *ptr);
void mca_vector_pop(struct mca_vector *v, size_t i);
void *mca_vector_get(struct mca_vector *v, size_t i);

#endif
