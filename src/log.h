/* See LICENSE file for copyright and license details */
#define LOGINFO(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__);} while (0)

#define LOGERROR(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__);} while (0)

#define LOGFATAL(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__);\
	exit(EXIT_FAILURE);} while (0)

/**
 * Log the current time in month-day-year format to the given stream.
 */
void
logtime(FILE *fp)
{
	char buf[16];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), "%m %d %T", tm);
	fprintf(fp, buf);
}
