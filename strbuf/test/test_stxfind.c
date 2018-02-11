#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

// Random strings generated for the test.
char rs1[1024];
char rb1[1024];
static const stx s1 = {
	.mem = rs1,
	.len = sizeof(rs1) - 1,
	.size = sizeof(rs1) - 1,
};
static const stx s2 = {
	.mem = rb1,
	.len = sizeof(rb1) - 1,
	.size = sizeof(rb1) - 1,
};

TEST_DEFINE(stxfind_mem_rand_str)
{
	size_t begin = test_rand(0, sizeof(rs1) - 1);
	size_t end = test_rand(begin, sizeof(rs1) - 1);

	spx slice = {
		.mem = rs1 + begin,
		.len = end - begin,
	};

	spx found = stxfind_mem(stxref(&s1), rs1 + begin, end - begin);
	TEST_ASSERT(found.mem == slice.mem);
	TEST_ASSERT(found.len == slice.len);

	TEST_END;
}

TEST_DEFINE(stxfind_mem_rand_bytes)
{
	size_t begin = test_rand(0, sizeof(rb1) - 1);
	size_t end = test_rand(begin, sizeof(rb1) - 1);

	spx slice = {
		.mem = rb1 + begin,
		.len = end - begin,
	};

	spx found = stxfind_mem(stxref(&s2), rb1 + begin, end - begin);
	TEST_ASSERT(found.mem == slice.mem);
	TEST_ASSERT(found.len == slice.len);

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	test_rand_str(rs1, sizeof(rs1));
	test_rand_bytes(rb1, sizeof(rb1));

	TEST_INIT(ts);
	TEST_RUN(ts, stxfind_mem_rand_str);
	TEST_RUN(ts, stxfind_mem_rand_bytes);
	TEST_PRINT(ts);
}
