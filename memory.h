#ifndef MEMORY_H
#define MEMORY_H

#include "status.h"

status_t sane_alloc (void* (*ptr), size_t count, size_t size);

#define typed_alloc(TYPE, PTR, COUNT) ({ \
	void* typed_alloc_tmp_ptr; \
	status_t typed_alloc_tmp_status = sane_alloc (&typed_alloc_tmp_ptr, COUNT, sizeof (TYPE)); \
	*(PTR) = (TYPE*) typed_alloc_tmp_ptr; \
	typed_alloc_tmp_status; \
})

#endif
