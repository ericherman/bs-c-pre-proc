#include <limits.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bs-cpp.h"

/* globals for testing */
FILE *bs_stdin = NULL;
#define BS_STDIN (bs_stdin ? bs_stdin : stdin)
FILE *bs_stdout = NULL;
#define BS_STDOUT (bs_stdout ? bs_stdout : stdout)
FILE *bs_stderr = NULL;
#define BS_STDERR (bs_stderr ? bs_stderr : stderr)

/* global function pointers for tests to intercept */
FILE *(*bs_fopen)(const char *restrict path, const char *restrict mode) = fopen;
int (*bs_fclose)(FILE *stream) = fclose;
void *(*bs_malloc)(size_t size) = malloc;
void (*bs_free)(void *ptr) = free;

/* local prototypes, not static for testing */
char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log);
int bs_include(FILE *out, char *buf, size_t bufsize, size_t offset, FILE *log);

int bs_preprocess(FILE *in, FILE *out, char *buf, size_t bufsize, FILE *log)
{
	assert(in);
	assert(out);
	assert(buf);
	assert(bufsize);
	assert(log);

	int continue_long_line = 0;

	// fgets() reads in at most one less than size characters from
	// stream and stores them into the buffer pointed to by s.
	// Reading stops after an EOF or a newline.
	// If a newline is read, it is stored into the buffer.
	// A terminating null byte is stored after the last character in
	// the buffer.
	while (fgets(buf, bufsize, in)) {
		size_t ws = 0;
		while (!continue_long_line && buf[ws] &&
		       (buf[ws] == ' ' || buf[ws] == '\t')) {
			++ws;
		}
		size_t offset = ws;
		char *from = buf + offset;
		int include_line = (!continue_long_line) &&
		    (!strncmp(from, "#include", strlen("#include")));

		if (include_line) {
			int error = bs_include(out, buf, bufsize, offset, log);
			if (error) {
				return error;
			}
		} else {
			int written = fprintf(out, "%s", buf);
			if (written < 0 || ((size_t)written) >= bufsize) {
				fprintf(log, "fprintf returned %d\n", written);
				return 2;
			}
			if ((((size_t)written) == (bufsize - 1))
			    && (buf[bufsize - 1] != '\n')) {
				continue_long_line = 1;
			} else {
				continue_long_line = 0;
			}
		}
	}
	return 0;
}

int bs_include(FILE *out, char *buf, size_t bufsize, size_t offset, FILE *log)
{
	char *name, *name_end;
	char delim1 = '"';
	char delim2 = '"';
	char *from = buf + offset;
	name = bs_name_from_include(from, delim1, delim2, &name_end, log);
	if (!name) {
		return 1;
	}
	// we will not process anything more on this line
	// NULL-terminate the string, so we can use it for the
	// path for fopen.
	*name_end = '\0';

	FILE *include = bs_fopen(name, "r");
	if (!include) {
		fprintf(log, "File not found: '%s'.\n", name);
		return 1;
	}

	buf[0] = '\0';
	int error = bs_preprocess(include, out, buf, bufsize, log);
	bs_fclose(include);

	return error;
}

char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log)
{
	assert(name_end);

	char *name = strchr(buf + strlen("#include") + 1, start_delim);
	if (name) {
		*name_end = strchr(name + 1, until_delim);
		if (!*name_end) {
			fprintf(log, "parse error"
				" #include has unbalanced quotes."
				" Opening quote but no close with: "
				" %s\n", buf);
			return NULL;
		}
		// skip the quote
		name += 1;
	}
	return name;
}

int bs_cpp(int argc, char **argv)
{
	FILE *in = NULL;
	FILE *out = NULL;
	char *buf = NULL;
	int error = 0;

	if (argc < 2) {
		fprintf(BS_STDERR, "usage: %s path/to/in.c path/to/out.c\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	in = bs_fopen(argv[1], "r");
	if (!in) {
		fprintf(BS_STDERR, "could not fopen(\"%s\", \"r\")\n", argv[1]);
		error = 1;
		goto bs_cpp_end;
	}
	out = bs_fopen(argv[2], "w");
	if (!out) {
		fprintf(BS_STDERR, "could not fopen(\"%s\", \"w\")\n", argv[2]);
		error = 1;
		goto bs_cpp_end;
	}

	const size_t longest_line_we_expect = 1000 + (2 * PATH_MAX);
	const size_t bufsize = longest_line_we_expect;
	buf = bs_malloc(bufsize);
	if (!buf) {
		fprintf(BS_STDERR, "could not malloc(%zu)\n", bufsize);
		error = 1;
		goto bs_cpp_end;
	}

	FILE *log = BS_STDERR;
	error = bs_preprocess(in, out, buf, bufsize, log);

bs_cpp_end:
	bs_free(buf);
	if (out) {
		bs_fclose(out);
	}
	if (in) {
		bs_fclose(in);
	}

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
