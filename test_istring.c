#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "istrlib.h"

void test_new_and_free() 
{
	istring *str = istr_new(NULL, 0);
	istr_free(str, true);

	str = istr_new("hello", 5);
	istr_free(str, true);
}

void test_assign_string()
{
	istring *str = istr_new(NULL, 0);

	str = istr_assign(str, "hello", 5);

	assert(strcmp(istr_str(str), "hello") == 0);
	assert(istr_len(str) == 5);

	str = istr_assign(str, "OH", 2);

	assert(!strcmp(istr_str(str), "OH") == 0);
	assert(istr_len(str) == 2);

	istr_free(str, true);
}

int main()
{
	test_new_and_free();
	test_assign_string();
	return 0;
}
