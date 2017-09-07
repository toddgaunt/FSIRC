/* See LICENSE file for copyright and license details */
struct arg_option {
	const char flag;
	const char *name;
	size_t argc;
	void *argv;
	void (*callback)();
	const char *default_arg;
	const char *help;
};

// Help message callback
void arg_help(const size_t optc, const struct arg_option *optv);

// Help functions.
void arg_usage(const size_t optc, const struct arg_option *optv);

// Provided callback functions. They define the specification for a libarg
// callback function. The first argument is usually the destination of the
// function's computed value, the second argument is the number of destination
// arguments, and the third argument is string-literals that gets processed.
// The return value is the number of arguments succesfully parsed. -1 is
// is returned upon an error during the callback.
void arg_setnot(const size_t argc, int *argv);
void arg_setflag(const size_t argc, int *argv);
void arg_setptr(const size_t argc, const char **argv, const char *str);
void arg_setlong(const size_t argc, long *argv, const char *str);
void arg_setdouble(const size_t argc, double *argv, const char *str);

char **arg_sort(char **argv);
char **arg_parse(char **argv, const size_t optc, const struct arg_option *optv);
