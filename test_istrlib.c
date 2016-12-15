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
	str = istr_new(two);
	istr_free(two, true);
	istr_free(str, true);

	str = istr_new_bytes(NULL, 0);
	istr_free(str, true);

	str = istr_new_bytes("hello", 6);
	istr_free(str, true);

	str = istr_new_cstr(NULL);
	istr_free(str, true);

	str = istr_new_cstr("hello");
	istr_free(str, true);
}

void test_assign()
{
	istring *str = istr_new(NULL);

	str = istr_assign_cstr(str, "hello");

	assert(strcmp(istr_str(str), "hello") == 0);
	assert(istr_len(str) == 6);

	str = istr_assign_cstr(str, "OH");

	assert(strcmp(istr_str(str), "OH") == 0);
	assert(istr_len(str) == 3);

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

int main()
{
	test_new_and_free();
	test_assign();
	test_istr_str();
	return 0;
}
