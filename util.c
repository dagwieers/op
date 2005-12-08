#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include "defs.h"

#if !defined(HAVE_VSNPRINTF)
#warning Your system does not support vsnprintf.
#warning This leaves op open to potential buffer overflows.
#endif

void vstrnprintf(char *out, int len, const char *format, va_list args) {
#ifdef HAVE_VSNPRINTF
	vsnprintf(out, len, format, args);
#else
	vsprintf(out, format, args);
#endif
}

void strnprintf(char *out, int len, const char *format, ...) {
va_list args;

	va_start(args, format);
#ifdef HAVE_VSNPRINTF
	vsnprintf(out, len, format, args);
#else
	vsprintf(out, format, args);
#endif
	va_end(args);
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

#undef malloc
void * rpl_malloc (size_t n) { if (n == 0) n = 1; return malloc (n); }
#undef realloc
void * rpl_realloc (void *ptr, size_t n) { if (n == 0) n = 1; return realloc (ptr, n); }
