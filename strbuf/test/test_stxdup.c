#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

// Random strings generated for the test.
char rs1[1024];
char rs2[1024];
char rb1[1024];
char rb2[1024];

TEST_DEFINE(stxdup_mem_zero)
{
	stx s1;

	TEST_ASSERT(0 == stxdup_mem(&s1, NULL, 0));
	TEST_ASSERT(NULL == s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxdup_mem_once)
{
	stx s1;

	TEST_ASSERT(0 == stxdup_mem(&s1, rb1, sizeof(rb1)));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rb1, sizeof(rb1)));
	TEST_ASSERT(sizeof(rb1) == s1.len);
	TEST_ASSERT(sizeof(rb1) == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxdup_mem_twice) {
	char *p;
	stx s1;

	TEST_ASSERT(0 == stxdup_mem(&s1, rb1, sizeof(rb1)));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rb1, sizeof(rb1)));
	TEST_ASSERT(sizeof(rb1) == s1.len);
	TEST_ASSERT(sizeof(rb1) == s1.size);

	p = s1.mem;

	TEST_ASSERT(0 == stxdup_mem(&s1, rb2, sizeof(rb2)));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rb2, sizeof(rb2)));
	TEST_ASSERT(sizeof(rb2) == s1.len);
	TEST_ASSERT(sizeof(rb2) == s1.size);

	stxfree(&s1);
	free(p);

	TEST_END;
}

TEST_DEFINE(stxdup_str_zero)
{
	stx s1 = {0};

	TEST_ASSERT(0 == stxdup_str(&s1, NULL));
	TEST_ASSERT(NULL == s1.mem);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxdup_str_once)
{
	stx s1;

	TEST_ASSERT(0 == stxdup_str(&s1, rs1));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rs1, strlen(rs1)));
	TEST_ASSERT(strlen(rs1) == s1.len);
	TEST_ASSERT(strlen(rs1) == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxdup_str_twice) {
	char *p;
	stx s1;

	TEST_ASSERT(0 == stxdup_str(&s1, rs1));
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rs1, strlen(rs1)));
	TEST_ASSERT(strlen(rs1) == s1.len);
	TEST_ASSERT(strlen(rs1) == s1.size);

	p = s1.mem;

	TEST_ASSERT(0 == stxdup_str(&s1, rs2));
	TEST_ASSERT(p != s1.mem);
	TEST_ASSERT(NULL != s1.mem);
	TEST_ASSERT(0 == memcmp(s1.mem, rs2, strlen(rs2)));
	TEST_ASSERT(strlen(rs2) == s1.len);
	TEST_ASSERT(strlen(rs2) == s1.size);

	stxfree(&s1);
	free(p);

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	test_rand_bytes(rb1, sizeof(rb1));
	test_rand_bytes(rb2, sizeof(rb2));
	test_rand_str(rs1, sizeof(rs1));
	test_rand_str(rs2, sizeof(rs2));

	TEST_INIT(ts);
	TEST_RUN(ts, stxdup_mem_zero);
	TEST_RUN(ts, stxdup_mem_once);
	TEST_RUN(ts, stxdup_mem_twice);
	TEST_RUN(ts, stxdup_str_zero);
	TEST_RUN(ts, stxdup_str_once);
	TEST_RUN(ts, stxdup_str_twice);
	TEST_PRINT(ts);
}
