#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include "defs.h"

#ifndef HAVE_VSNPRINTF
#warning You have not compiled op with vsnprintf
#warning support, presumably because your system
#warning does not have it. This leaves op open
#warning to potential buffer overflows.
#endif

#ifdef HAVE_SNPRINTF
#error "Now using 'vsnprintf' instead of snprintf. Adjust your build to define HAVE_VSNPRINTF."
#endif

void strnprintf(char *out, int len, const char *format, va_list args) {

#ifdef HAVE_VSNPRINTF
	vsnprintf(out, len, format, args);
#else
	vsprintf(out, format, args);
#endif
}

char *strtolower(char *in) {
char *i;

	for (i = in; *i; ++i) *i = tolower(*i);
	return in;
}

array_t *array_alloc() {
array_t *array = malloc(sizeof(array_t));

	if (!array || !(array->data = malloc(sizeof(void**) * ARRAY_CHUNK)))
		fatal(1, "failed to allocate array");
	array->capacity = ARRAY_CHUNK;
	array->size = 0;
	return array;
}

void array_free(array_t *array) {
	free(array->data);
	free(array);
}

array_t *array_free_contents(array_t *array) {
int i;

	for (i = 0; i < array->size; ++i)
		free(array->data[i]);
	array->size = 0;
	return array;
}

void *array_push(array_t *array, void *object) {
	if (array->size + 1 >= array->capacity) {
		array->capacity += ARRAY_CHUNK;
		if (!(array->data = realloc(array->data, sizeof(void**) * array->capacity)))
			fatal(1, "failed to extend array");
	}
	return (array->data[array->size++] = object);
}

void *array_pop(array_t *array) {
	if (array->size <= 0) return NULL;
	return array->data[--array->size];
}

int array_extend(array_t *array, int capacity) {
	if (capacity < array->capacity) return 0;
	array->capacity = capacity;
	array->data = realloc(array->data, sizeof(void**) * array->capacity);
	return 1;
}
