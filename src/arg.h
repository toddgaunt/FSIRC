#ifndef LIBARG_H
#define LIBARG_H

#include <stdbool.h>

struct arg_option {
	const char flag;
	const char *name;
	void *parv;
	size_t parc;
	void (*callback)();
	const char *default_arg;
	const char *help;
};

// Help message callback
void arg_help(const struct arg_option *opt, size_t optc);

// Help functions.
void arg_usage(const struct arg_option *opt, size_t optc);

// Provided callback functions. They define the specification for a libarg
// callback function. The first argument is usually the destination of the
// function's computed value, the second argument is the number of destination
// arguments, and the third argument is string-literals that gets processed.
// The return value is the number of arguments succesfully parsed. -1 is
// is returned upon an error during the callback.
void arg_not(int *parv, size_t parc);
void arg_flag(int *parv, size_t parc);
void arg_assign_ptr(char **parv, size_t parc, char *arg);
void arg_assign_long(long *parv, size_t parc, char *arg);
void arg_assign_double(double *par, size_t parc, char *arg);

char **arg_sort(char **argv);
char **arg_parse(char **argv, const struct arg_option *opt, size_t optc);

#endif
