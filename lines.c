#include "lines.h"
#include <stdbool.h>

status_t make_line (line_t (*line))
{
	line->ntbs = NULL;
	line->size = 0;
	return success ();
}

status_t resize_line (line_t (*line), size_t size)
{
	if (line->size >= size)
		return success ();

	void* new_data = realloc (line->ntbs, size);
	if (new_data == NULL)
		return failure ("Could not allocate memory");

	line->ntbs = new_data;
	line->size = size;
	return success ();
}

status_t empty_line (line_t (*line))
{
	status_t status = resize_line (line, 1);
	if (failed (status))
		return status;

	line->ntbs [0] = '\0';
	return success ();
}

status_t read_segment (line_t (*line),
                       FILE* file,
                       fpos_t begin,
                       size_t length)
{
	line_t _line = (*line);
	status_t status = resize_line (&_line, length + 1);
	if (failed (status))
		return status;

	if (fsetpos (file, &begin) != 0)
		return efailure (errno);

	if (fgets (_line.ntbs, length + 1, file) != _line.ntbs)
		return failure ("Read error");

	(*line) = _line;
	return success ();
}

status_t get_line (line_t (*line), FILE* file)
{
	fpos_t begin;
	if (fgetpos (file, &begin) != 0)
		return efailure (errno);

	size_t length = 0;
	bool eof = false;
	for (bool done = false; !done;) {
		switch (fgetc (file)) {
			case EOF:
				if (ferror (file) != 0)
					return failure ("Read error");
				eof = true;
				done = true;
				break;

			case '\n':
				done = true;
				break;

			default:
				++length;
				break;
		}
	}

	if (eof) {
		if (length != 0)
			return read_segment (line, file, begin, length);
		else
			return empty_line (line);
	}
	else {
		status_t status = read_segment (line, file, begin, length);
		if (!failed (status))
			fgetc (file); // Discard newline
		return status;
	}
}
