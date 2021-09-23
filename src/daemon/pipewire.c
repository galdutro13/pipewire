/* PipeWire
 *
 * Copyright © 2018 Wim Taymans
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

#include <limits.h>
#include <signal.h>
#include <getopt.h>
#include <libgen.h>

#include <spa/utils/result.h>
#include <pipewire/pipewire.h>

#include <pipewire/i18n.h>

#include "config.h"

static void do_quit(void *data, int signal_number)
{
	struct pw_main_loop *loop = data;
	pw_main_loop_quit(loop);
}

static void show_help(const char *name, const char *config_name)
{
	fprintf(stdout, _("%s [options]\n"
		"  -h, --help                            Show this help\n"
		"      --version                         Show version\n"
		"  -c, --config                          Load config (Default %s)\n"),
		name,
		config_name);
}

int main(int argc, char *argv[])
{
	struct pw_context *context = NULL;
	struct pw_main_loop *loop = NULL;
	struct pw_properties *properties = NULL;
	static const struct option long_options[] = {
		{ "help",	no_argument,		NULL, 'h' },
		{ "version",	no_argument,		NULL, 'V' },
		{ "config",	required_argument,	NULL, 'c' },
		{ "verbose",	no_argument,		NULL, 'v' },

		{ NULL, 0, NULL, 0}
	};
	int c, res = 0;
	char path[PATH_MAX];
	const char *config_name;
	enum spa_log_level level = pw_log_level;

	if (setenv("PIPEWIRE_INTERNAL", "1", 1) < 0)
		fprintf(stderr, "can't set PIPEWIRE_INTERNAL env: %m");

	snprintf(path, sizeof(path), "%s.conf", argv[0]);
	config_name = basename(path);

	pw_init(&argc, &argv);

	while ((c = getopt_long(argc, argv, "hVc:v", long_options, NULL)) != -1) {
		switch (c) {
		case 'v':
			if (level < SPA_LOG_LEVEL_TRACE)
				level++;
			break;
		case 'h':
			show_help(argv[0], config_name);
			return 0;
		case 'V':
			fprintf(stdout, "%s\n"
				"Compiled with libpipewire %s\n"
				"Linked with libpipewire %s\n",
				argv[0],
				pw_get_headers_version(),
				pw_get_library_version());
			return 0;
		case 'c':
			config_name = optarg;
			break;
		default:
			res = -EINVAL;
			goto done;
		}
	}
	pw_log_set_level(level);

	properties = pw_properties_new(
				PW_KEY_CONFIG_NAME, config_name,
				NULL);

	loop = pw_main_loop_new(&properties->dict);
	if (loop == NULL) {
		pw_log_error("failed to create main-loop: %m");
		res = -errno;
		goto done;
	}

	pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT, do_quit, loop);
	pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM, do_quit, loop);

	context = pw_context_new(pw_main_loop_get_loop(loop), properties, 0);
	properties = NULL;

	if (context == NULL) {
		pw_log_error("failed to create context: %m");
		res = -errno;
		goto done;
	}

	pw_log_info("start main loop");
	pw_main_loop_run(loop);
	pw_log_info("leave main loop");

done:
	pw_properties_free(properties);
	if (context)
		pw_context_destroy(context);
	if (loop)
		pw_main_loop_destroy(loop);
	pw_deinit();

	return res;
}
