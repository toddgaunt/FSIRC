#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

TEST_DEFINE(stxavail_none)
{
	stx s1 = {0};
	TEST_ASSERT(0 == stxavail(&s1));

	TEST_END;
}

TEST_DEFINE(stxavail_max) {
	size_t n = test_rand(0, 65535);

	stx s1 = {
		.mem = NULL,
		.len = n,
		.size = n,
	};

	TEST_ASSERT(0 == stxavail(&s1));

	TEST_END;
}

TEST_DEFINE(stxavail_half) {
	size_t n = test_rand(0, 65535);

	if (n % 2)
		++n;

	stx s1 = {
		.mem = NULL,
		.len = n / 2,
		.size = n,
	};

	TEST_ASSERT(n / 2 == stxavail(&s1));

	TEST_END;
}

TEST_DEFINE(stxavail_rand) {
	size_t n1 = test_rand(0, 65535);
	size_t n2 = test_rand(n1, 65535);

	stx s1 = {
		.mem = NULL,
		.len = n1,
		.size = n2,
	};

	TEST_ASSERT(s1.size - s1.len == stxavail(&s1));

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	TEST_INIT(ts);
	TEST_RUN(ts, stxavail_none);
	TEST_RUN(ts, stxavail_max);
	TEST_RUN(ts, stxavail_half);
	TEST_RUN(ts, stxavail_rand);
	TEST_PRINT(ts);
}
