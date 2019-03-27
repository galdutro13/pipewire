/* Spa
 *
 * Copyright © 2019 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <spa/debug/mem.h>

#include "resample.h"
#include "resample-native.h"

#define N_SAMPLES	253
#define N_CHANNELS	11

static float samp_in[N_SAMPLES * 4];
static float samp_out[N_SAMPLES * 4];

static void feed_1(struct resample *r)
{
	uint32_t i;
	const void *src[1];
	void *dst[1];

	spa_zero(samp_out);
	src[0] = samp_in;
	dst[0] = samp_out;

	for (i = 0; i < 500; i++) {
		uint32_t in, out;

		in = out = 1;
		samp_in[0] = i;
		resample_process(r, src, &in, dst, &out);
		fprintf(stderr, "%d %d %f %d\n", i, in, samp_out[0], out);
	}
}

static void test_native(void)
{
	struct resample r;

	r.channels = 1;
	r.i_rate = 44100;
	r.o_rate = 44100;
	impl_native_init(&r);

	feed_1(&r);

	r.channels = 1;
	r.i_rate = 44100;
	r.o_rate = 48000;
	impl_native_init(&r);

	feed_1(&r);
}

int main(int argc, char *argv[])
{
	test_native();

	return 0;
}
