#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

TEST_DEFINE(stxgrow_zero)
{
	stx s1 = {0};

	TEST_ASSERT(0 == stxgrow(&s1, 0));
	TEST_ASSERT(NULL == s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	TEST_END;
}

TEST_DEFINE(stxgrow_max)
{
	stx s1 = {0};

	TEST_ASSERT(0 == stxgrow(&s1, 65535));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(65535 == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxgrow_twice_rand)
{
	stx s1 = {0};

	size_t n1 = test_rand(0, 65535/2);
	size_t n2 = test_rand(n1, 65535);

	TEST_ASSERT(0 == stxgrow(&s1, n1));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(n1 == s1.size);

	TEST_ASSERT(0 == stxgrow(&s1, n2));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(n1 + n2 == s1.size);

	stxfree(&s1);

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	TEST_INIT(ts);
	TEST_RUN(ts, stxgrow_zero);
	TEST_RUN(ts, stxgrow_max);
	TEST_RUN(ts, stxgrow_twice_rand);
	TEST_PRINT(ts);
}
