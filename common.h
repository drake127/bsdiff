/*-
 * Copyright 2018-2020 Emanuel Komínek
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BSDIFF_COMMON_H
#define BSDIFF_COMMON_H

#ifndef min
# define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

inline GCC_NORETURN void errx(int eval, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(eval);
}

static inline void read_file_to_buffer(const char * path, uint8_t ** output_buffer, int64_t * output_size)
{
	FILE * f;
	struct stat64 s;
	size_t bytes_read = 0, to_read;

	if ((f = fopen(path, "rb")) == NULL)
		errx(1, "fopen (%s)", path);
	
	if (fstat64(fileno(f), &s) == -1)
		errx(1, "fstat (%s)", path);
	*output_size = s.st_size;

	if ((*output_buffer = malloc(*output_size + 1)) == NULL)
		errx(1, "malloc (%lld bytes)", output_size + 1);

	while ((to_read = min(*output_size - bytes_read, 1048576)) != 0)
	{
		if (fread(*output_buffer + bytes_read, 1, to_read, f) != to_read)
			errx(1, "fread (%s)", path);
		bytes_read += to_read;
	}

	if (fclose(f) != 0)
		errx(1, "fclose (%s)", path);
}

static inline void write_buffer_to_file(const char * path, uint8_t * output_buffer, int64_t output_size)
{
	FILE * f;
	size_t bytes_written = 0, to_write;

	if ((f = fopen(path, "wb")) == NULL)
		errx(1, "fopen (%s)", path);
	
	while ((to_write = min(output_size - bytes_written, 1048576)) != 0)
	{
		if (fwrite(output_buffer + bytes_written, 1, to_write, f) != to_write)
			errx(1, "fwrite (%s)", path);
		bytes_written += to_write;
	}

	if (fclose(f) != 0)
		errx(1, "fclose (%s)", path);
}

#endif
