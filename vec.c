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
