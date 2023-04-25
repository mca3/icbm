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

/* mca_vector_new creates a new instance of mca_vector.
 *
 * On error, NULL is returned.
 */
struct mca_vector *
mca_vector_new(size_t cap)
{
	struct mca_vector *new = malloc(sizeof(struct mca_vector));
	if (!new)
		return NULL;

	memset(new, 0, sizeof(struct mca_vector));
	return new;
}

/* mca_vector_free frees the vector.
 *
 * You should iterate through the vector and clean up all data before calling
 * this, especially when you have dynamically allocated data.
 */
void
mca_vector_free(struct mca_vector *v)
{
	free(v->data);
	free(v);
}

/* Ensures that there is enough room for another "n" elements.
 * If there is not enough room, an allocation is attempted.
 *
 * On error, -1 is returned.
 */
int
mca_vector_ensure(struct mca_vector *v, size_t n)
{
	size_t new_size = v->len + n;

	if (new_size > v->cap) {
		void **newdata = realloc(v->data, sizeof(void *)*new_size);
		if (!newdata)
			return -1;

		v->data = newdata;
	}

	return 0;
}

/* mca_vector_push appends something to the end of the vector.
 * The index to where the value was put is returned.
 *
 * Upon error, -1 is returned.
 */
size_t
mca_vector_push(struct mca_vector *v, void *ptr)
{
	if (mca_vector_ensure(v, 1) == -1)
		return -1;

	v->data[v->len++] = ptr;
	return v->len-1;
}

/* Removes an element from the vector.
 *
 * If i is less than 0, then the last element is popped off.
 * Otherwise, the element at index i will be removed and all elements
 * proceeding it moved back.
 *
 * An assertion is raised if the vector's length is zero.
 */
void
mca_vector_pop(struct mca_vector *v, size_t i)
{
	assert(v->len > 0);

	if (i == -1) {
		v->data[--v->len] = NULL;
		return;
	}

	memmove(v->data + i, v->data + i + 1, (v->len - i - 1) * sizeof(void *));
	v->len--;
}

/* Fetches an element from the vector.
 *
 * An assertion is raised if the index is greater or equal to the length of the
 * vector, as you can not access data in either case.
 * */
void *
mca_vector_get(struct mca_vector *v, size_t i)
{
	assert(i < v->len);

	return v->data[i];
}
