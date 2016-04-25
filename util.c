/* +-------------------------------------------------------------------+ */
/* | Copyright 1991, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include "defs.h"
#include <errno.h>

char *
strtolower(char *in)
{
    char *i;

    for (i = in; *i; ++i)
	*i = tolower(*i);
    return in;
}

array_t *
array_alloc(void)
{
    array_t *array = malloc(sizeof(array_t));

    if (!array || !(array->data = malloc(sizeof(void **) * ARRAY_CHUNK)))
	fatal(1, "failed to allocate array");
    array->capacity = ARRAY_CHUNK;
    array->size = 0;
    return array;
}

void
array_free(array_t * array)
{
    free(array->data);
    free(array);
}

array_t *
array_free_contents(array_t * array)
{
    size_t i;

    for (i = 0; i < array->size; ++i)
	free(array->data[i]);
    array->size = 0;
    return array;
}

void *
array_push(array_t * array, void *object)
{
    if (array->size + 1 >= array->capacity) {
	array->capacity += ARRAY_CHUNK;
	if (!
	    (array->data =
	     realloc(array->data, sizeof(void **) * array->capacity)))
	    fatal(1, "failed to extend array");
    }
    return (array->data[array->size++] = object);
}

void *
array_pop(array_t * array)
{
    if (array->size == 0)
	return NULL;
    return array->data[--array->size];
}

int
array_extend(array_t * array, size_t capacity)
{
    if (capacity < array->capacity)
	return 0;
    array->capacity = capacity;
    array->data = realloc(array->data, sizeof(void **) * array->capacity);
    return 1;
}

#ifdef malloc
#undef malloc
void *
rpl_malloc(size_t n)
{
    if (n == 0)
	n = 1;
    return malloc(n);
}
#endif

#ifdef realloc
#undef realloc
void *
rpl_realloc(void *ptr, size_t n)
{
    if (n == 0)
	n = 1;
    return realloc(ptr, n);
}
#endif

/* from man strtol(1) */
/* NOLINTNEXTLINE(runtime/int) */
long
strtolong(const char *str, int base)
{
    char *endptr;
    /* NOLINTNEXTLINE(runtime/int) */
    long val;

    errno = 0;			/* To distinguish success/failure after call */
    val = strtol(str, &endptr, base);	/* base 10 */

    /* Check for various possible errors */

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
	|| (errno != 0 && val == 0))
	fatal(1, "Number out of range");

    if (endptr == str)
	fatal(1, "No digits were found");

    if (val < 0)
	fatal(1, "Number out of range");

    return val;
}
