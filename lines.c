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

status_t read_segment (line_t (*line),
                       FILE* file,
                       fpos_t begin,
                       fpos_t end)
{
	line_t _line = (*line);
	status_t status = resize_line (_line, end - begin + 1);
	if (failed (status))
		return status;

	if (fsetpos (file, &begin) != 0)
		return efailure (errno);

	if (fgets (_line->ntbs, end - begin + 1, file) != _line->ntbs)
		return failure ("Read error");

	(*line) = _line;
	return success ();
}

status_t get_line (line_t (*line), FILE* file)
{
	fpos_t begin;
	if (fgetpos (file, &begin) != 0)
		return efailure (errno);

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
				break;
		}
	}

	fpos_t end;
	if (fgetpos (file, &end) != 0)
		return efailure (errno);

	if (eof)
		return read_segment (line, file, begin, end);
	else {
		status_t status = read_segment (line, file, begin, end - 1);
		if (!failed (status))
			fgetc (file); // Discard newline
		return status;
	}
}
