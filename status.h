#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum status_type {
	STATUS_SUCCESS,
	STATUS_FAILURE
} status_type;

typedef struct status_t {
	status_type type;
	const char* reason;
} status_t;

static inline
status_t success (void)
{
	return (status_t) {
		.type   = STATUS_SUCCESS,
		.reason = NULL
	};
}

static inline
status_t failure (const char* reason)
{
	return (status_t) {
		.type   = STATUS_FAILURE,
		.reason = reason
	};
}

static inline
status_t efailure (int err)
{
	return failure (strerror (err));
}

static inline
int failed (status_t s)
{
	return s.type == STATUS_FAILURE;
}

void check (status_t status, const char* fmt, ...);
void raw_check (bool good, const char* fmt, ...);

#endif
