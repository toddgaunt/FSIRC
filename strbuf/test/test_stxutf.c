#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../libstx.h"
#include "test.h"

TEST_DEFINE(stxutf8len_1through4)
{
	spx str = {10, "a£ก𐊀"};
	TEST_ASSERT(4 == stxutf8len(str));
	TEST_END;
}

TEST_DEFINE(stxutf8n32_1byte)
{
	for (uint32_t wc=0; wc<128; ++wc) {
		TEST_ASSERT(1 == stxutf8n32(wc));
	}

	TEST_END;
}

TEST_DEFINE(stxutf8n32_2byte)
{
	for (uint32_t wc=0x80; wc<0x800; ++wc) {
		TEST_ASSERT(2 == stxutf8n32(wc));
	}

	TEST_END;
}

TEST_DEFINE(stxutf8n32_3byte)
{
	for (uint32_t wc=0x800; wc<0x10000; ++wc) {
		TEST_ASSERT(3 == stxutf8n32(wc));
	}

	TEST_END;
}

TEST_DEFINE(stxutf8n32_4byte)
{
	for (uint32_t wc=0x10000; wc<0x11000; ++wc) {
		TEST_ASSERT(4 == stxutf8n32(wc));
	}

	TEST_END;
}

TEST_DEFINE(stxutf8n32_outside_range_quick)
{
	for (uint32_t wc=0x11000; wc<0xFFFFF; ++wc) {
		TEST_ASSERT(0 == stxutf8n32(wc));
	}

	TEST_END;
}

TEST_DEFINE(stxutf8f32_1byte)
{
	char set_1byte[128];
	char set_target[128];

	for (uint32_t wc=0; wc<128; ++wc) {
		set_target[wc] = wc;
		TEST_ASSERT(1 == stxutf8f32(set_1byte + wc, wc));
	}

	TEST_ASSERT(0 == memcmp(set_target, set_1byte, sizeof(set_target)));

	TEST_END;
}

TEST_DEFINE(stxutf8f32_2byte)
{
	char set_2byte[(0x800 - 0x80) * 2];
	char set_target[(0x800 - 0x80) * 2];

	for (uint32_t wc=0x80; wc<0x800; ++wc) {
		uint32_t i = (wc - 0x80) * 2;
		set_target[i] = (0x06 << 5) | (wc >> 6);
		set_target[i + 1] = (0x02 << 6) | (wc & 0x3F);
		TEST_ASSERT(2 == stxutf8f32(set_2byte + i, wc));
	}

	TEST_ASSERT(0 == memcmp(set_target, set_2byte, sizeof(set_2byte)));

	TEST_END;
}

TEST_DEFINE(stxutf8f32_3byte)
{
	char set_3byte[(0x10000 - 0x800) * 3];
	char set_target[(0x10000 - 0x800) * 3];

	for (uint32_t wc=0x800; wc<0x10000; ++wc) {
		uint32_t i = (wc - 0x800) * 3;
		set_target[i] = (0x0E << 4) | (wc >> 12);
		set_target[i + 1] = (0x02 << 6) | ((wc >> 6) & 0x3F);
		set_target[i + 2] = (0x02 << 6) | (wc & 0x3F);
		TEST_ASSERT(3 == stxutf8f32(set_3byte + i, wc));
	}

	TEST_ASSERT(0 == memcmp(set_target, set_3byte, sizeof(set_3byte)));

	TEST_END;
}

TEST_DEFINE(stxutf8f32_4byte)
{
	char set_4byte[(0x11000 - 0x10000) * 4];
	char set_target[(0x11000 - 0x10000) * 4];

	for (uint32_t wc=0x10000; wc<0x11000; ++wc) {
		uint32_t i = (wc - 0x10000) * 4;
		set_target[i] = (0x1E << 3) | (wc >> 18);
		set_target[i + 1] = (0x02 << 6) | ((wc >> 12) & 0x3F);
		set_target[i + 2] = (0x02 << 6) | ((wc >> 6 ) & 0x3F);
		set_target[i + 3] = (0x02 << 6) | (wc & 0x3F);
		TEST_ASSERT(4 == stxutf8f32(set_4byte + i, wc));
	}

	TEST_ASSERT(0 == memcmp(set_target, set_4byte, sizeof(set_4byte)));

	TEST_END;
}

int
main(void)
{
	srand(time(NULL));
	TEST_INIT(ts);
	TEST_RUN(ts, stxutf8n32_1byte);
	TEST_RUN(ts, stxutf8n32_2byte);
	TEST_RUN(ts, stxutf8n32_3byte);
	TEST_RUN(ts, stxutf8n32_4byte);
	TEST_RUN(ts, stxutf8n32_outside_range_quick);
	TEST_RUN(ts, stxutf8f32_1byte);
	TEST_RUN(ts, stxutf8f32_2byte);
	TEST_RUN(ts, stxutf8f32_3byte);
	TEST_RUN(ts, stxutf8f32_4byte);
	TEST_RUN(ts, stxutf8len_1through4);
	TEST_PRINT(ts);
}
