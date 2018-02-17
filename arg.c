#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "arg.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void
arg_setflag(int *dest)
{
	*dest = 1;
}

void
arg_setnot(int *dest)
{
	*dest = !*dest;
}

void
arg_setstr(char const **dest, char const *str)
{
	*dest = str;
}

void
arg_setlong(long *dest, char const *str)
{
	*dest = strtol(str, NULL, 10);
}

void
arg_setdouble(double *dest, char const *str)
{
	*dest = strtod(str, NULL);
}

void
arg_print_usage(size_t optc, ArgOption const *optv)
{
	fprintf(stderr, "usage: [-h, --help]");
	for (size_t i = 0; i < optc; ++i) {
		if (optv[i].flag && optv[i].name) {
			fprintf(stderr, " [-%c, --%s", optv[i].flag, optv[i].name);
		} else if (optv[i].flag) {
			fprintf(stderr, " [-%c", optv[i].flag);
		} else if (optv[i].name) {
			fprintf(stderr, " [--%s", optv[i].name);
		} else {
			fprintf(stderr, "Error printing option list\n");
			exit(-1);
		}
		// The flag requires an argument list
		if (optv[i].default_arg) {
			if (optv[i].name) {
				fprintf(stderr, " <%s>", optv[i].name);
			} else if (optv[i].flag) {
				fprintf(stderr, " <%c>", optv[i].flag);
			} else {
				fprintf(stderr, "Error printing option list\n");
				exit(-1);
			}
		}
		putc(']', stderr);
	}
}

void
arg_print_help(size_t optc, ArgOption const *optv)
{
	size_t i;
	size_t j;
	/* Default pad length is the size of the help flag and message */
	size_t maxlen = 10;
	size_t pad = 0;

	fprintf(stderr, "options:\n");
	for (i = 0; i < optc; ++i) {
		if (optv[i].flag && optv[i].name) {
			maxlen = MAX(maxlen, 6 + strlen(optv[i].name));
		} else if (optv[i].flag) {
			maxlen = MAX(maxlen, 2);
		} else if (optv[i].name) {
			maxlen = MAX(maxlen, 2 + strlen(optv[i].name));
		} else {
			printf("\nError printing help\n");
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "\t-h, --help");
	for (j = 0; j < maxlen - 10 + 2; ++j) {
		putc(' ', stderr);
	}
	fprintf(stderr, "Display this help message and exit\n");
	for (i = 0; i < optc; ++i) {
		if (optv[i].flag && optv[i].name) {
			pad = 6 + strlen(optv[i].name);
			fprintf(stderr, "\t-%c, --%s", optv[i].flag, optv[i].name);
		} else if (optv[i].flag) {
			pad = 2;
			fprintf(stderr, "\t-%c", optv[i].flag);
		} else if (optv[i].name) {
			pad = 2 + strlen(optv[i].name);
			fprintf(stderr, "\t--%s", optv[i].name);
		} else {
			printf("\nError printing help\n");
			exit(EXIT_FAILURE);
		}

		if (optv[i].help) {
			for (j = 0; j < maxlen - pad + 2; ++j) {
				putc(' ', stderr);
			}
			fprintf(stderr, "%s\n", optv[i].help);
		} else {
			 putc('\n', stderr);
		}
	}
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
		const ArgOption *optv,
		void (*usage)(size_t, ArgOption const *),
		void (*help)(size_t, ArgOption const *)
		)
{
	size_t i;
	int len;

	for (i = 0; i < optc; ++i) {
		len = strlen(optv[i].name);
		/* Built-in '--help' flag */
		if (0 == strncmp("help", pp[0], len))
			help(optc, optv);
		/* Check to see if the long-name matches any options */
		if (0 != strncmp(optv[i].name, pp[0], len))
			continue;
		pp[0] += len;
		if ('\0' != pp[0][0] && '=' != pp[0][0]) {
				fprintf(stderr, "invalid option \"--%s\"\n",
						pp[0] - len);
				usage(optc, optv);
		}
		if (optv[i].default_arg) {
			if ('=' == pp[0][0]) {
				++pp[0];
				if ('\0' == pp[0][0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					optv[i].name);
					usage(optc, optv);
				}
				optv[i].callback(optv[i].dest, pp[0]);
			} else {
				++pp;
				if ('\0' == pp[0]) {
					fprintf(stderr,
					"option \"--%s\" requires an argument\n",
					optv[i].name);
					usage(optc, optv);
				}
				optv[i].callback(optv[i].dest, pp[0]);
			}
		} else {
			if (optv[i].dest) {
				optv[i].callback(optv[i].dest);
			} else {
				optv[i].callback();
			}
		}
		called[i] = true;
		return pp + 1;
	}
	fprintf(stderr, "invalid option \"--%s\"\n", pp[0]);
	usage(optc, optv);
	return pp + 1;
}

static char **
opt_flag_callback(
		char **pp,
		bool *called,
		size_t optc,
		const ArgOption *optv,
		void (*usage)(size_t, ArgOption const *),
		void (*help)(size_t, ArgOption const *)
		)
{
	size_t i;

	for (i = 0; i < optc; ++i) {
		/* Built-in '-h' flag */
		if ('h' == pp[0][0])
			help(optc, optv);
		/* Check to see if the flag matches any of the options, or if
		 * the option even has a flag */
		if (pp[0][0] != optv[i].flag && '\0' != optv[i].flag)
			continue;
		++pp[0];
		if (optv[i].default_arg) {
			if ('\0' == pp[0][0]) {
				++pp;
				if (!pp[0]) {
					fprintf(stderr,
					"option \"-%c\" requires an argument\n",
					optv[i].flag);
					usage(optc, optv);
				}
				optv[i].callback(optv[i].dest, pp[0]);
			} else {
				optv[i].callback(optv[i].dest, pp[0]);
				++pp;
			}
		} else {
			if (optv[i].dest) {
				optv[i].callback(optv[i].dest);
			} else {
				optv[i].callback();
			}
		}
		called[i] = true;
		return pp;
	}
	fprintf(stderr, "invalid option \"-%c\"\n", pp[0][0]);
	usage(optc, optv);
	return pp;
}

char **
arg_parse(
		char **argv,
		size_t optc,
		ArgOption const *optv,
		void (*usage)(size_t, ArgOption const *),
		void (*help)(size_t, ArgOption const *)
	)
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
				pp = opt_name_callback(pp, called, optc, optv,
						usage, help);
			} else {
				char **tmp = pp;
				while (pp == tmp && pp[0][0]) {
					pp = opt_flag_callback(pp, called, optc, optv,
							usage, help);
				}
				++pp;
			}
		} else {
			// Non-option encountered.
			break;
		}
	}
	// Call uncalled options with default arguments.
	for (size_t i = 0; i < optc; ++i) {
		if (!called[i] && optv[i].default_arg) {
			optv[i].callback(optv[i].dest,
					 optv[i].default_arg);
		}
	}
	// All options have been processed.
	return pp;
}
