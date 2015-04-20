#include "status.h"
#include <stdarg.h>
#include <stdio.h>

void check (status_t status, const char* fmt, ...)
{
	if (failed (status)) {
		va_list args;
		va_start (args, fmt);
		vfprintf (stderr, fmt, args);
		va_end (args);
		fprintf (stderr, "\n");

		if (status.reason != NULL)
			fprintf (stderr, "Reason: %s\n", status.reason);

		exit (EXIT_FAILURE);
	}
}

void raw_check (bool good, const char* fmt, ...)
{
	if (!good) {
		va_list args;
		va_start (args, fmt);
		vfprintf (stderr, fmt, args);
		va_end (args);
		fprintf (stderr, "\n");

		exit (EXIT_FAILURE);
	}
}
