﻿/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012-2018 Matthew Endsley
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

#include <limits.h>
#include "bspatch.h"

// Converts signed magnitude to two's complement.
static inline void offtin(int64_t * x)
{
	if (*x < 0)
		*x = (~*x + 1) | INT64_MIN;
}

int bspatch(const uint8_t * source, const int64_t sourcesize, uint8_t * target,
            const int64_t targetsize, struct bspatch_stream * stream)
{
	int64_t oldpos = 0, newpos = 0;
	int64_t ctrl[3];

	while (newpos < targetsize)
	{
		// Reads control data block.
		if (stream->read(stream, ctrl, sizeof(ctrl), BSDIFF_READCONTROL))
			return -1;
		for (int i = 0; i <= 2; ++i)
			offtin(ctrl + i);

		// Checks sanity of diff control data.
		if (ctrl[0] < 0 || ctrl[0] > targetsize - newpos)
			return -1;

		// Reads diff data block.
		if (stream->read(stream, target + newpos, ctrl[0], BSDIFF_READDIFF))
			return -1;

		// Adds old data to the diff data.
		if (oldpos < 0 || oldpos + ctrl[0] < 0 || oldpos + ctrl[0] > sourcesize)
			return -1;
		for(int64_t i = 0; i < ctrl[0]; ++i)
			target[newpos+i] += source[oldpos+i];

		// Adjusts position pointers.
		newpos += ctrl[0];
		oldpos += ctrl[0];

		// Checks sanity of extra control data.
		if(ctrl[1] < 0 || ctrl[1] > targetsize - newpos)
			return -1;

		// Reads extra data block.
		if (stream->read(stream, target + newpos, ctrl[1], BSDIFF_READEXTRA))
			return -1;

		// Adjust position pointers.
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	return 0;
}

#if defined(BSPATCH_EXECUTABLE)

#include <bzlib.h>
#include <string.h>
#include "bsdiff_common.h"

static int bz2_read(const struct bspatch_stream * stream, void * buffer,
                    size_t length, ATTR_UNUSED enum bspatch_stream_type type)
{
	size_t bytes_read = 0;
	int to_read;
	while ((to_read = (int)min(length - bytes_read, 1048576)) != 0)
	{
		int bz2err;
		if (BZ2_bzRead(&bz2err, (BZFILE *)stream->opaque, (uint8_t *)buffer + bytes_read, to_read) != to_read)
			return -1;
		bytes_read += to_read;
	}

	return 0;
}

int main(int argc, char * argv[])
{
	FILE * fp;
	BZFILE * bz2;
	int bz2err;
	uint8_t header[24];
	uint8_t * source, * target;
	int64_t sourcesize, targetsize;
	struct bspatch_stream stream;

	// Usage
	if(argc != 4)
		errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

	// Opens patch file
	if ((fp = fopen(argv[3], "rb")) == NULL)
		errx(1, "fopen (%s)\n", argv[3]);

	// Reads bsdiff header
	if (fread(header, 1, 24, fp) != 24)
	{
		if (feof(fp))
			errx(1, "Corrupt patch header\n");
		errx(1, "fread (%s)\n", argv[3]);
	}

	// Checks for appropriate magic
	if (memcmp(header, "ENDSLEY/BSDIFF43", 16) != 0)
		errx(1, "Corrupt patch header (magic)\n");

	// Reads target size from header
	targetsize = *(int64_t *)(header+16);
	if(targetsize < 0)
		errx(1, "Corrupt patch header (target size)\n");

	// Allocates target buffer.
	if ((target = malloc(targetsize + 1)) == NULL)
		errx(1, "malloc (%lld bytes)", targetsize + 1);

	// Opens bzip2 stream.
	if ((bz2 = BZ2_bzReadOpen(&bz2err, fp, 0, 0, NULL, 0)) == NULL)
		errx(1, "BZ2_bzReadOpen (bz2err: %d)", bz2err);

	// Opens and reads source file.
	read_file_to_buffer(argv[1], &source, &sourcesize);

	// Applies patch.
	stream.read = bz2_read;
	stream.opaque = bz2;
	if (bspatch(source, sourcesize, target, targetsize, &stream))
		errx(1, "bspatch");

	// Closes patch file.
	BZ2_bzReadClose(&bz2err, bz2);
	if (bz2err != BZ_OK)
		errx(1, "BZ2_bzReadClose (bz2err: %d)", bz2err);
	if (fclose(fp) != 0)
		errx(1, "fclose (%s)", argv[3]);

	// Writes the new file.
	write_buffer_to_file(argv[2], target, targetsize);

	free(source);
	free(target);
	
	return 0;
}

#endif
