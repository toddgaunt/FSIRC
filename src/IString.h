#ifndef IString_H_INCLUDED
#define IString_H_INCLUDED

#define BASE_STR_LEN 16

#include <stdlib.h>

typedef struct IString;

IString* IString_new();

void IString_free(IString *string);

IString *IString_assign(GString *string, const char *str, size_t n);

IString* IString_dup(const IString *string);

#endif
