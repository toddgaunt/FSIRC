#ifndef IString_H_INCLUDED
#define IString_H_INCLUDED

#define BASE_STR_LEN 16

#include <stdlib.h>
#include <stdbool.h>

typedef struct IString {
	char *buf;     // character buffer
	size_t size;   // size of character buffer
	size_t len; // amount of characters until '\0'
} IString;

IString* IString_new(const char *str, size_t len);

char* IString_free(IString *string, bool free_buf);

IString* IString_assign(IString *string, const char *str, size_t len);

IString* IString_dup(const IString *string);

IString* IString_appends(IString *string, const char *str, size_t len);

int IString_eq(const IString *s1, const IString *s2);

#endif
