/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include "bs-cpp.h"
#include "test-util.h"

#include <string.h>

char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log);

unsigned test_name_from_include_basic(void)
{
	unsigned failures = 0;

	const size_t bufsize = 80 * 24;
	char logbuf[80 * 24];
	memset(logbuf, 0x00, bufsize);
	FILE *log = fmemopen(logbuf, bufsize, "w");

	char buf[80];
	strncpy(buf, "#include \"foo.h\"\n", 80);

	char delim1 = '"';
	char delim2 = '"';

	char *name_end = NULL;
	char *name = bs_name_from_include(buf, delim1, delim2, &name_end, log);

	failures += Check(name_end, "name is NULL\n");
	if (name_end) {
		*name_end = '\0';
	}

	failures += Check(strcmp("foo.h", name) == 0, "name is NULL\n");

	fflush(log);
	fclose(log);
	log = NULL;
	failures += Check((strcmp(logbuf, "") == 0), "error log: %s\n", logbuf);

	return failures;
}

unsigned test_name_from_include_malformed(void)
{
	unsigned failures = 0;

	const size_t bufsize = 80 * 24;
	char logbuf[80 * 24];
	memset(logbuf, 0x00, bufsize);
	FILE *log = fmemopen(logbuf, bufsize, "w");

	char buf[80];
	strncpy(buf, "#include \"malformed.h>\n", 80);

	char delim1 = '"';
	char delim2 = '"';

	char *name_end = NULL;
	char *name = bs_name_from_include(buf, delim1, delim2, &name_end, log);

	failures += Check(!name_end, "name_end expected NULL, but was: '%s'\n",
			  name_end);
	if (name_end) {
		*name_end = '\0';
	}

	failures += Check(!name, "name expected NULL, but was: '%s'\n", name);

	fflush(log);
	fclose(log);
	log = NULL;
	failures += Check(strstr(logbuf, "error"), "error log: %s\n", logbuf);

	return failures;
}

int main(void)
{
	unsigned failures = 0;

	failures += run_test(test_name_from_include_basic);
	failures += run_test(test_name_from_include_malformed);

	return failures_to_status("test_exit_reason", failures);
}
