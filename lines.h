#ifndef LINES_H
#define LINES_H

#include "status.h"
#include <stddef.h>
#include <stdio.h>

typedef struct line_t {
	char*  ntbs;
	size_t size;
} line_t;

status_t make_line (line_t (*line));
status_t get_line (line_t (*line), FILE* file);
void free_line (line_t (*line));

#endif
