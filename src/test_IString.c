#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "IString.h"

void test_new_and_free() 
{
	IString *string = IString_new("hello", 5);
	printf("string: \"%s\"\nsize: %d\nlen: %d\n", string->buf, string->size, string->len);
	IString_free(string, true);
}

int main(int argc, char **argv)
{
	test_new_and_free();
	return 0;
}

