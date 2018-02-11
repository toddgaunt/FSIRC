/* See LICENSE file for copyright and license details */
#ifndef TEST_H
#define TEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define TEST_NAME_MAX_LEN 59

#define TEST_INIT(handle) struct test_stat handle = {0}

#define TEST_DEFINE(name) int name (void)

#define TEST_ASSERT(expr) \
	do { \
		if (!(expr)) { \
			printf("(\x1B[31mFAIL\x1B[0m) (%s:%d: \"%s\")\n", \
					__FILE__, __LINE__, #expr); \
			return -1; \
		} \
	} while (0)

#define TEST_RUN(handle, name) test_run(&(handle), #name, name)

#define TEST_END return 0

#define TEST_PRINT(handle) \
	do { \
		if ((handle).passed || (handle).failed) { \
			printf("Passed: %d\n", (handle).passed); \
			printf("Failed: %d\n", (handle).failed); \
			printf("Total:  %d\n", (handle).passed + (handle).failed); \
		} \
		return (handle).failed; \
	} while (0)

struct test_stat {
	int passed;
	int failed;
};

void test_align(int len);
int test_max(int a, int b);
void test_run(struct test_stat *ts, const char *name, int (*call)(void));
size_t test_rand(size_t start, size_t end);
void test_rand_bytes(void *mem, size_t n);
void test_rand_str(char *str, size_t n);

void
test_align(int len)
{
	for (int i=0; i<len; ++i) {
		putchar('.');
	}
}

int
test_max(int a, int b)
{
	return a > b ? a : b;
}

// NOTE: Not a truly random range, but good enough.
size_t
test_rand(size_t start, size_t end)
{
	return start + (rand() % (end - start + 1));
}

void
test_rand_bytes(void *mem, size_t n)
{
	char *bytes = mem;
	for (size_t i=0; i<n; ++i) {
		bytes[i] = (rand() % 256);
	}
}

void
test_rand_str(char *str, size_t n)
{
	for (size_t i=0; i<n - 1; ++i) {
		str[i] = 1 + (rand() % 255);
	}
	str[n-1] = '\0';
}

void
test_run(struct test_stat *ts, const char *name, int (*call)(void)) {
	int namelen = strlen(name);

	if (namelen > TEST_NAME_MAX_LEN) {
		printf("- Testing ...%s ", name + namelen + 3 - TEST_NAME_MAX_LEN);
	} else {
		printf("- Testing %s ", name);
	}

	int ret = call();

	if (ret) {
		++(ts->failed);
	} else {
		test_align(test_max(TEST_NAME_MAX_LEN - namelen + 3, 3));
		printf(" (\x1B[32mOK\x1B[0m)\n");
		++(ts->passed);
	}
}
#endif
