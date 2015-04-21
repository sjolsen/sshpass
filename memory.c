#include "memory.h"
#include <stdlib.h>
#include <stdint.h>

status_t sane_alloc (void* (*ptr), size_t count, size_t size)
{
	size_t max_count = SIZE_MAX / size;
	if (count > max_count)
		return failure ("Allocation failure");

	void* new_mem = malloc (size * count);
	if (new_mem == NULL)
		return failure ("Allocation failure");

	(*ptr) = new_mem;
	return success ();
}
