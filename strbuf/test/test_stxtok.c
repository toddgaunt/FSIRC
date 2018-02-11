#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

static char b1[] = "hello worlds with delimits";
static char b2[] = "hello\0\1world\0\1num\0\1delimits";
static const stx s1 = {
	.mem = b1,
	.len = sizeof(b1) - 1,
	.size = sizeof(b1) - 1,
};
static const stx s2 = {
	.mem = b2,
	.len = sizeof(b2) - 1,
	.size = sizeof(b2) - 1,
};

TEST_DEFINE(stxtok_chs_null)
{
	spx ref = stxref(&s1);
	spx tok = stxtok(&ref, "\0", 1);

	TEST_ASSERT(tok.mem == ref.mem);
	TEST_ASSERT(tok.len == ref.len);

	TEST_END;
}

TEST_DEFINE(stxtok_chs_space)
{
	spx tok;
	spx ref;
	
	ref = stxref(&s1);

	tok = stxtok(&ref, " ", 1);
	TEST_ASSERT(0 == memcmp(tok.mem, "hello", 5));
	TEST_ASSERT(tok.len == 5);

	tok = stxtok(&ref, " ", 1);
	TEST_ASSERT(0 == memcmp(tok.mem, "worlds", 6));
	TEST_ASSERT(tok.len == 6);

	tok = stxtok(&ref, " ", 1);
	TEST_ASSERT(0 == memcmp(tok.mem, "with", 4));
	TEST_ASSERT(tok.len == 4);

	tok = stxtok(&ref, " ", 1);
	TEST_ASSERT(0 == memcmp(tok.mem, "delimits", 8));
	TEST_ASSERT(tok.len == 8);

	TEST_END;
}

TEST_DEFINE(stxtok_chs_null_one)
{
	TEST_END;
}

int
main(void)
{
	TEST_INIT(ts);
	TEST_RUN(ts, stxtok_chs_null);
	TEST_RUN(ts, stxtok_chs_space);
	//TEST_RUN(ts, stxtok_chs_null_one);
	TEST_PRINT(ts);
}
