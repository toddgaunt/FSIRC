#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arg.h"

void
arg_setflag(size_t argc, int *argv)
{
	for (size_t i = 0; i < argc; ++i) {
			argv[i] = 1;
	}
}

void
arg_setnot(size_t argc, int *argv)
{
	for (size_t i = 0; i < argc; ++i) {
			argv[i] = !argv[i];
	}
}

void
arg_setptr(const size_t argc, const char **argv, const char *str)
{
	for (size_t i = 0; i < argc; ++i) {
			argv[i] = str;
	}
}

void
arg_setlong(const size_t argc, long *argv, const char *str)
{
	if (argc > 0) {
		long res = strtol(str, NULL, 0);
		for (size_t i=0; i<argc; ++i) {
				argv[i] = res;
		}
	}
}

void
arg_setdouble(const size_t argc, double *argv, const char *str)
{
	if (argc > 0) {
		double res = strtod(str, NULL);
		for (size_t i = 0; i < argc; ++i) {
				argv[i] = res;
		}
	}
}

static void
print_optlist(const size_t optc, const struct arg_option *opt)
{
	fprintf(stderr, "usage: ");

	for (size_t i = 0; i < optc; ++i) {
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
		if (opt[i].default_arg) {
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
arg_help(const size_t optc, const struct arg_option *optv)
{
	print_optlist(optc, optv);

	fprintf(stderr, "options:\n");
	for (size_t i = 0; i < optc; ++i) {
		if (optv[i].flag && optv[i].name) {
			fprintf(stderr, "\t-%c, --%s", optv[i].flag, optv[i].name);
		} else if (optv[i].flag) {
			fprintf(stderr, "\t-%c", optv[i].flag);
		} else if (optv[i].name) {
			fprintf(stderr, "\t--%s", optv[i].name);
		} else {
			printf("\nError printing help\n");
			exit(EXIT_FAILURE);
		}

		if (optv[i].help) {
			fprintf(stderr, "\t%s\n", optv[i].help);
		} else {
			 putc('\n', stderr);
		}
	}

	exit(EXIT_FAILURE);
}

void
arg_usage(const size_t optc, const struct arg_option *optv)
{
	print_optlist(optc, optv);
	exit (EXIT_FAILURE);
}

char **
arg_sort(char **argv)
{
	return argv;
}

static char **
opt_name_callback(
		char **pp,
		bool *called,
		size_t optc,
		const struct arg_option *optv
		)
{
	size_t i;
	for (i = 0; i < optc; ++i) {
		int len = strlen(optv[i].name);
		if (0 != strncmp(optv[i].name, pp[0], len))
			continue;

		pp[0] += len;

		if ('\0' != pp[0][0] && '=' != pp[0][0]) {
				fprintf(stderr, "invalid option \"--%s\"\n",
						pp[0] - len);
				arg_usage(optc, optv);
		}

		if (optv[i].default_arg) {
			if ('=' == pp[0][0]) {
				++pp[0];

				if ('\0' == pp[0][0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					optv[i].name);
					arg_usage(optc, optv);
				}

				optv[i].callback(optv[i].argc, optv[i].argv, pp[0]);
			} else {
				++pp;

				if (!pp[0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					optv[i].name);
					arg_usage(optc, optv);
				}

				optv[i].callback(optv[i].argc, optv[i].argv, pp[0]);
			}
		} else {
			if (optv[i].argv) {
				optv[i].callback(optv[i].argc, optv[i].argv);
			} else {
				optv[i].callback();
			}
		}

		called[i] = true;
		return pp;
	}

	fprintf(stderr, "invalid option \"--%s\"\n", pp[0]);
	arg_usage(optc, optv);
}

static char **
opt_flag_callback(
		char **pp,
		bool *called,
		size_t optc,
		const struct arg_option *optv
		)
{
	for (size_t i = 0; i < optc; ++i) {
		if (pp[0][0] != optv[i].flag)
			continue;

		++pp[0];

		if (optv[i].default_arg) {
			if ('\0' == pp[0][0]) {
				++pp;
				if (!pp[0]) {
					fprintf(stderr,
					"option \"-%c\" requires an argument\n",
					optv[i].flag);
					arg_usage(optc, optv);
				}
			}

			optv[i].callback(optv[i].argc, optv[i].argv, pp[0]);
			++pp;
		} else {
			if (optv[i].argv) {
				optv[i].callback(optv[i].argc, optv[i].argv);
			} else {
				optv[i].callback();
			}
		}

		called[i] = true;
		return pp;
	}

	fprintf(stderr, "invalid option \"-%c\"\n", pp[0][0]);
	arg_usage(optc, optv);
}

char **
arg_parse(char **argv, const size_t optc, const struct arg_option *optv)
{
	// Array of flags indicating if a callback was called or not.
	bool called[optc];
	// Iterator through argv.
	char **pp = argv;

	memset(called, 0, sizeof(called));
	while (pp[0] && pp[0][0]) {
		// Argument termination.
		if (0 == strcmp(pp[0], "--"))
			return pp;

		if ('-' == pp[0][0]) {
			++pp[0];
			if ('-' == pp[0][0]) {
				++pp[0];
				pp = opt_name_callback(pp, called, optc, optv);
			} else {
				char **tmp = pp;
				while (pp == tmp && pp[0][0]) {
					pp = opt_flag_callback(pp, called, optc, optv);
				}
			}
		} else {
			// Non-option encountered.
			break;
		}
	}

	// Call uncalled options with default arguments.
	for (size_t i = 0; i < optc; ++i) {
		if (!called[i] && optv[i].default_arg) {
			optv[i].callback(optv[i].argc,
					 optv[i].argv,
					 optv[i].default_arg);
		}
	}

	// All options have been processed.
	return pp;
}
