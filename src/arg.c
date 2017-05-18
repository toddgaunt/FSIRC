#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libarg.h"

void
arg_flag(int *parv, size_t parc)
{
	for (size_t i=0; i<parc; ++i) {
			parv[i] = 1;
	}
}

void
arg_not(int *parv, size_t parc)
{
	for (size_t i=0; i<parc; ++i) {
			parv[i] = !parv[i];
	}
}

void
arg_assign_ptr(char **parv, size_t parc, char *str)
{
	for (size_t i=0; i<parc; ++i) {
			parv[i] = str;
	}
}

void
arg_assign_long(long *parv, size_t parc, char *str)
{
	if (parc > 0) {
		long res = strtol(str, NULL, 0);
		for (size_t i=0; i<parc; ++i) {
				parv[i] = res;
		}
	}
}

void
arg_assign_double(double *parv, size_t parc, char *str)
{
	if (parc > 0) {
		double res = strtod(str, NULL);
		for (size_t i=0; i<parc; ++i) {
				parv[i] = res;
		}
	}
}

static void
print_optlist(const struct arg_option *opt, size_t optc)
{
	fprintf(stderr, "usage: ");

	for (size_t i=0; i<optc; ++i) {
		if (opt[i].flag && opt[i].name) {
			fprintf(stderr, " [-%c, --%s", opt[i].flag, opt[i].name);
		} else if (opt[i].flag) {
			fprintf(stderr, " [-%c", opt[i].flag);
		} else if (opt[i].name) {
			fprintf(stderr, " [--%s", opt[i].name);
		} else {
			fprintf(stderr, "Error printing option list\n");
			exit(-1);
		}

		// The flag requires an argument list
		if (opt[i].arg) {
			if (opt[i].name) {
				fprintf(stderr, " <%s>", opt[i].name);
			} else if (opt[i].flag) {
				fprintf(stderr, " <%c>", opt[i].flag);
			} else {
				fprintf(stderr, "Error printing option list\n");
				exit(-1);
			}
		}

		putc(']', stderr);
	}

	putc('\n', stderr);
}

void
arg_help(const struct arg_option *opt, size_t optc)
{
	print_optlist(opt, optc);

	fprintf(stderr, "options:\n");
	for (size_t i=0; i<optc; ++i) {
		if (opt[i].flag && opt[i].name) {
			fprintf(stderr, "\t-%c, --%s", opt[i].flag, opt[i].name);
		} else if (opt[i].flag) {
			fprintf(stderr, "\t-%c", opt[i].flag);
		} else if (opt[i].name) {
			fprintf(stderr, "\t--%s", opt[i].name);
		} else {
			printf("\nError printing help\n");
			exit(EXIT_FAILURE);
		}

		if (opt[i].help) {
			fprintf(stderr, "\t%s\n", opt[i].help);
		} else {
			 putc('\n', stderr);
		}
	}

	exit(EXIT_FAILURE);
}

void
arg_usage(const struct arg_option *opt, size_t optc)
{
	print_optlist(opt, optc);
	exit (EXIT_FAILURE);
}

char **
arg_sort(char **argv)
{
	//TODO(todd): insertion sort.
	return argv;
}

static char **
opt_name_callback(char **pp, const struct arg_option *opt, size_t optc)
{
	for (size_t i=0; i<optc; ++i) {
		int len = strlen(opt[i].name);
		if (0 != strncmp(opt[i].name, pp[0], len))
			continue;

		pp[0] += len;

		if ('\0' != pp[0][0] && '=' != pp[0][0]) {
				fprintf(stderr, "invalid option \"--%s\"\n",
						pp[0] - len);
				arg_usage(opt, optc);
		}

		if (ARG_REQUIRED == opt[i].arg) {
			if ('=' == pp[0][0]) {
				++pp[0];

				if ('\0' == pp[0][0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					opt[i].name);
					arg_usage(opt, optc);
				}

				opt[i].callback(opt[i].parv, opt[i].parc, pp[0]);
			} else {
				++pp;

				if (!pp[0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					opt[i].name);
					arg_usage(opt, optc);
				}

				opt[i].callback(opt[i].parv, opt[i].parc, pp[0]);
			}
		} else {
			if (opt->parv) {
				opt[i].callback(opt->parv, opt->parc);
			} else {
				opt[i].callback();
			}
		}
		goto success;
	}

	fprintf(stderr, "invalid option \"--%s\"\n", pp[0]);
	arg_usage(opt, optc);

success:
	return pp;
}

static char **
opt_flag_callback(char **pp, const struct arg_option *opt, size_t optc)
{
	for (size_t i=0; i<optc; ++i) {
		if (pp[0][0] != opt[i].flag)
			continue;

		++pp[0];

		if (ARG_REQUIRED == opt[i].arg) {
			if ('\0' == pp[0][0]) {
				++pp;
				if (!pp[0]) {
					fprintf(stderr,
					"option \"-%c\" requires an argument\n",
					opt[i].flag);
					arg_usage(opt, optc);
				}
			}

			opt[i].callback(opt[i].parv, opt[i].parc, pp[0]);
			++pp;
		} else {
			if (opt->parv) {
				opt[i].callback(opt->parv, opt->parc);
			} else {
				opt[i].callback();
			}
		}

		goto success;
	}

	fprintf(stderr, "invalid option \"-%c\"\n", pp[0][0]);
	arg_usage(opt, optc);

success:
	return pp;
}

char **
arg_parse(char **argv, const struct arg_option *opt, size_t optc)
{
	char **pp = argv;
	while (pp[0] && pp[0][0]) {
		// Argument termination.
		if (0 == strcmp(pp[0], "--"))
			return pp;

		if ('-' == pp[0][0]) {
			++pp[0];
			if ('-' == pp[0][0]) {
				++pp[0];
				pp = opt_name_callback(pp, opt, optc);
			} else {
				char **tmp = pp;
				while (pp == tmp && pp[0][0]) {
					pp = opt_flag_callback(pp, opt, optc);
				}
			}
		} else {
			// Non-option encountered.
			return pp;
		}
	}

	// All options have been processed.
	return pp;
}
