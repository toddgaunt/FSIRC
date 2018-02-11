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

TEST_DEFINE(stxapp_mem_zero)
{
	stx s1 = {0};

	TEST_ASSERT(&s1 == stxapp_mem(&s1, NULL, 0));
	TEST_ASSERT(s1.mem == NULL);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	TEST_END;
}

TEST_DEFINE(stxapp_mem_once)
{
	stx s1;
	stxalloc(&s1, sizeof(rb1));

	TEST_ASSERT(&s1 == stxapp_mem(&s1, rb1, sizeof(rb1)));
	TEST_ASSERT(s1.mem != NULL);
	TEST_ASSERT(0 == memcmp(s1.mem, rb1, sizeof(rb1)));
	TEST_ASSERT(sizeof(rb1) == s1.len);
	TEST_ASSERT(sizeof(rb1) == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxapp_mem_bytes_seperately)
{
	stx s1;

	stxalloc(&s1, sizeof(rb1));

	for (size_t i=0; i<sizeof(rb1); ++i) {
		TEST_ASSERT(&s1 == stxapp_mem(&s1, &rb1[i], 1));
		TEST_ASSERT(s1.mem != NULL);
		TEST_ASSERT(0 == memcmp(s1.mem, rb1, i + 1));
		TEST_ASSERT(i + 1 == s1.len);
		TEST_ASSERT(sizeof(rb1) == s1.size);
	}

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxapp_mem_twice)
{
	stx s1;
	char tstr[sizeof(rb1) + sizeof(rb2)];

	memcpy(tstr, rb1, sizeof(rb1));
	memcpy(tstr + sizeof(rb1), rb2, sizeof(rb2));
	const size_t tlen = sizeof(rb1) + sizeof(rb2);

	stxalloc(&s1, sizeof(rb1) + sizeof(rb2));

	TEST_ASSERT(&s1 == stxapp_mem(&s1, rb1, sizeof(rb1)));
	TEST_ASSERT(sizeof(rb1) == s1.len);
	TEST_ASSERT(tlen == s1.size);

	TEST_ASSERT(&s1 == stxapp_mem(&s1, rb2, sizeof(rb2)));
	TEST_ASSERT(s1.mem != NULL);
	TEST_ASSERT(0 == memcmp(s1.mem, tstr, tlen));
	TEST_ASSERT(tlen == s1.len);
	TEST_ASSERT(tlen == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxapp_str_zero)
{
	stx s1 = {0};

	TEST_ASSERT(&s1 == stxapp_str(&s1, NULL));
	TEST_ASSERT(s1.mem == NULL);
	TEST_ASSERT(0 == s1.len);
	TEST_ASSERT(0 == s1.size);

	TEST_END;
}

TEST_DEFINE(stxapp_str_once)
{
	stx s1;

	stxalloc(&s1, strlen(rs1));

	TEST_ASSERT(&s1 == stxapp_str(&s1, rs1));
	TEST_ASSERT(s1.mem != NULL);
	TEST_ASSERT(0 == memcmp(s1.mem, rs1, strlen(rs1)));
	TEST_ASSERT(strlen(rs1) == s1.len);
	TEST_ASSERT(strlen(rs1) == s1.size);

	stxfree(&s1);

	TEST_END;
}

TEST_DEFINE(stxapp_str_twice)
{
	stx s1;
	char tstr[sizeof(rs1) + sizeof(rs2)];

	strcpy(tstr, rs1);
	strcpy(tstr + strlen(rs1), rs2);
	const size_t tlen = strlen(tstr);
	stxalloc(&s1, strlen(rs1) + strlen(rs2));

	TEST_ASSERT(&s1 == stxapp_str(&s1, rs1));
	TEST_ASSERT(strlen(rs1) == s1.len);
	TEST_ASSERT(tlen == s1.size);

	TEST_ASSERT(&s1 == stxapp_str(&s1, rs2));
	TEST_ASSERT(s1.mem != NULL);
	TEST_ASSERT(0 == memcmp(s1.mem, tstr, tlen));
	TEST_ASSERT(tlen == s1.len);
	TEST_ASSERT(tlen == s1.size);

	stxfree(&s1);

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
	TEST_RUN(ts, stxapp_mem_zero);
	TEST_RUN(ts, stxapp_mem_once);
	TEST_RUN(ts, stxapp_mem_bytes_seperately);
	TEST_RUN(ts, stxapp_mem_twice);
	TEST_RUN(ts, stxapp_str_zero);
	TEST_RUN(ts, stxapp_str_once);
	TEST_RUN(ts, stxapp_str_twice);
	TEST_PRINT(ts);
}
