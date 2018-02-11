#include "strbuf.h"
#include "test.h"

TEST_DEFINE(sb_init_macro)
{
	struct strbuf s1 = SB_INIT;

	TEST_ASSERT('\0' == s1.mem[0]);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	sb_release(&s1);

	TEST_END;
}

TEST_DEFINE(sb_init_zero)
{
	struct strbuf s1;

	sb_init(&s1, 0);

	TEST_ASSERT('\0' == s1.mem[0]);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	sb_release(&s1);

	TEST_END;
}

TEST_DEFINE(sb_init_max_16bit)
{
	struct strbuf s1;

	sb_init(&s1, 65536);

	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(65536 <= s1.size);

	sb_release(&s1);

	TEST_END;
}

TEST_DEFINE(sb_init_rand_twice)
{
	struct strbuf s1;
	char *p;
	size_t n1 = test_rand(1, 65536);
	size_t n2 = test_rand(1, 65536);
	sb_init(&s1, n1);

	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(n1 <= s1.size);

	p = s1.mem;
	sb_init(&s1, n2);

	TEST_ASSERT(p != s1.mem);
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(n2 <= s1.size);

	sb_release(&s1);
	free(p);

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));

	TEST_INIT(ts);
	TEST_RUN(ts, sb_init_zero);
	TEST_RUN(ts, sb_init_max_16bit);
	TEST_RUN(ts, sb_init_rand_twice);
	TEST_PRINT(ts);
}
