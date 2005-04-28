#include <stdlib.h>
#include "defs.h"

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
