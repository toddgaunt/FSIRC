/* See LICENSE file for copyright and license details */
#include <stdio.h>
#include <time.h>

#include "src/log.h"
	
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
