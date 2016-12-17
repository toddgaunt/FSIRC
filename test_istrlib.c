#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "istrlib.h"

#define TEST(name) void name ## () \
{

void test_new_and_free() 
{
	istring *str;
	istring *two;

	str = istr_new(NULL);
	istr_free(str, true);

	two = istr_new_cstr("hi");
	assert(strcmp(istr_str(two), "hi") == 0);
	assert(istr_len(two) == 3);
	assert(istr_size(two) >= 3);

	str = istr_new(two);
	assert(strcmp(istr_str(str), "hi") == 0);
	assert(istr_len(str) == 3);
	assert(istr_size(str) >= 3);

	istr_free(two, true);
	istr_free(str, true);

	str = istr_new_bytes(NULL, 0);
	assert(istr_len(str) == 0);
	assert(istr_size(str) >= 0);
	istr_free(str, true);

	str = istr_new_bytes("hello", 6);
	istr_free(str, true);

	str = istr_new_cstr(NULL);
	istr_free(str, true);

	str = istr_new_cstr("hello");
	istr_free(str, true);
}

void test_istr_str()
{
	istring *n = istr_new_cstr("hello");
	char *tmp;

	tmp = istr_str(n);
	tmp[2] = 'Z';
	tmp[3] = 'Z';

	n = istr_assign_cstr(n, "Wow bigger string will resize buffer, now that old pointer is dead...");

	tmp = istr_str(n);
	tmp[2] = 'Z';
	tmp[3] = 'Z';

	istr_free(n, true);
}

static inline void assign_assert(istring *string, char *str)
{
	size_t str_len = strlen(str) + 1;
	string = istr_assign_cstr(string, str);

	assert(strcmp(istr_str(string), str) == 0);
	assert(istr_len(string) == str_len);
	assert(istr_size(string) >= str_len);
}

void test_assign()
{
	istring *string = istr_new(NULL);

	// case1: assign a cstr to a NULL initialized istring
	assign_assert(string, "hello");
	// case2: assign a cstr to an already assigned istring
	assign_assert(string, "OH");
	// case3: assign a larger cstr to an istring to expand the size
	assign_assert(string, "WOWOWOWOWOWOWOWOWOW BIG STRING IS BIG");

	istr_free(string, true);
}

static inline void insert_assert(istring *string, size_t index, char *str, size_t str_len, char *outcome)
{
	size_t expected_len = istr_len(string) + str_len;
	string = istr_insert_bytes(string, index, str, str_len);

	assert(strcmp(istr_str(string), outcome) == 0);
	assert(istr_len(string) == expected_len);
	assert(istr_size(string) >= expected_len);
}

void test_insert()
{
	istring *string = istr_new_bytes("hello", 5);

	// case1: append
	insert_assert(string, 5, "omg", 4, "helloomg");

	// case2: prepend
	insert_assert(string, 0, "omg", 3, "omghelloomg");

	// case3: in the middle
	insert_assert(string, 6, "omg", 3, "omghelomgloomg");
	
	istr_free(string, true);
}

int main()
{
	printf("Testing istrlib...\n");
	test_new_and_free();
	test_istr_str();
	test_assign();
	test_insert();
	printf("istrlib testing success!\n");
	return 0;
}
