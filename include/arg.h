/* See LICENSE file for copyright and license details */
struct arg_option {
	/* Short name of option */
	char const flag;
	/* Long name of option */
	char const *name;
	/* Callback function to execute when option is encountered */ 
	void (*callback)();
	/* Destination to save parse result */
	void *dest;
	/* Null terminated list of strings */
	char const *default_arg;
	/* Help message displayed on calls to arg_help(). */
	char const *help;
};

/* Use these to construct your help() and usage() functions. */
/* Help message utility */
void arg_printhelp(size_t optc, struct arg_option const *optv);
/* Usage message utility */
void arg_print_usage(size_t optc, struct arg_option const *optv);

// Provided callback functions. They define the specification for a libarg
// callback function. The first argument is usually the destination of the
// function's computed value, the second argument is the number of destination
// arguments, and the third argument is string-literals that gets processed.
// The return value is the number of arguments succesfully parsed. -1 is
// is returned upon an error during the callback.
void arg_setnot(int *dest);
void arg_setflag(int *dest);
void arg_setptr(void const **dest, char const *argv);
void arg_setlong(long *dest, char const *argv);
void arg_setdouble(double *dest, char const *argv);

char **arg_parse(
		char **argv,
		size_t optc,
		struct arg_option const *optv,
		void (*usage)(size_t, struct arg_option const *),
		void (*help)(size_t, struct arg_option const *)
		);
