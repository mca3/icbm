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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

struct mca_vector *
mca_vector_new(size_t cap)
{
	struct mca_vector *new = malloc(sizeof(struct mca_vector));
	if (!new)
		return NULL;

	memset(new, 0, sizeof(struct mca_vector));
	return new;
}

void
mca_vector_free(struct mca_vector *v)
{
	free(v->data);
	free(v);
}

/* Ensures that there is enough room for size elements.
 * If there is not enough room, an allocation is attempted.
 *
 * On error, -1 is returned.
 */
int
mca_vector_ensure(struct mca_vector *v, size_t size)
{
	if (size > v->cap) {
		void **newdata = realloc(v->data, sizeof(void *)*size);
		if (!newdata)
			return -1;

		v->data = newdata;
	}

	return 0;
}

size_t
mca_vector_push(struct mca_vector *v, void *ptr)
{
	if (mca_vector_ensure(v, v->len+1) == -1)
		return -1;

	v->data[v->len++] = ptr;
	return v->len;
}

void
mca_vector_pop(struct mca_vector *v, size_t i)
{
	// TODO: use i
	assert(v->len > 0);
	v->data[--v->len] = NULL;
}

void *
mca_vector_get(struct mca_vector *v, size_t i)
{
	assert(i < v->len);

	return v->data[i];
}
