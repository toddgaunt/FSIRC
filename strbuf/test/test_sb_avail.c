#include "strbuf.h"
#include "test.h"

TEST_DEFINE(sb_avail_none)
{
	struct strbuf s1 = SB_INIT;

	TEST_ASSERT(0 == sb_avail(&s1));

	TEST_END;
}

TEST_DEFINE(sb_avail_max) {
	size_t n = test_rand(0, 65535);

	struct strbuf s1 = {
		.mem = NULL,
		.len = n,
		.size = n,
	};

	TEST_ASSERT(0 == sb_avail(&s1));

	TEST_END;
}

TEST_DEFINE(sb_avail_half) {
	size_t n = test_rand(0, 65535);

	if (n % 2)
		++n;

	struct strbuf s1 = {
		.mem = NULL,
		.len = n / 2,
		.size = n,
	};

	TEST_ASSERT(n / 2 == sb_avail(&s1));

	TEST_END;
}

TEST_DEFINE(sb_avail_rand) {
	size_t n1 = test_rand(0, 65535);
	size_t n2 = test_rand(n1, 65535);

	struct strbuf s1 = {
		.mem = NULL,
		.len = n1,
		.size = n2,
	};

	TEST_ASSERT(s1.size - s1.len == sb_avail(&s1));

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	TEST_INIT(ts);
	TEST_RUN(ts, sb_avail_none);
	TEST_RUN(ts, sb_avail_max);
	TEST_RUN(ts, sb_avail_half);
	TEST_RUN(ts, sb_avail_rand);
	TEST_PRINT(ts);
}
