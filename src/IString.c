#include <string.h>
#include <stdlib.h>

typedef struct IString {
	char *buf;     // character buffer
	size_t size;   // size of character buffer
	size_t len; // amount of characters until '\0'
} IString;

IString* IString_new() 
{
	IString *string = malloc(sizeof(string));
	string->buf = NULL;
	string->size = 0;
	string->len = 0;
}

void IString_free(IString *string)
{
	if (NULL != string->buf) {
		free(string->buf);
	}
	string->size = 0;
	string->len = 0;
}

IString* IString_assign(GString *string, const char *str, size_t n)
{
	if (NULL == string) {
		errno = EINVAL;
		return NULL;
	}
	
	if (NULL == string->buf) {
		string->buf = malloc(sizeof(string->buf)*BASE_STR_LEN);
		string->size = BASE_STR_LEN;
		string->len = 0;
	}

	size_t i;
	for (i=0;'\0' != str[i] || i < n;i++) {
		string->buf[i] = str[i];
		i++;
		if (string->size < i) {
			// Double the allocated memory
			string->size *= 2;
			string->buf = realloc(string->buf, sizeof(string->buf) * (string->size));
			if (!string->buf) {
				errno = ENOMEM;
				return NULL;
			}
		}
	}
	string->buf[i] = '\0';
	string->len = i;
	return string;
}

IString* IString_dup (const IString *string) 
{
	if (NULL == string) return NULL;

	IString *dup = malloc(sizeof(dup));
	if (!dup) {
		errno = ENOMEM;
		return NULL;
	}
	dup->buf = malloc(sizeof(dup->buf) * string->size);
	if (!dup->buf) {
		free(dup);
		errno = ENOMEM;
		return NULL;
	}
	dup->size = string->size;
	memcpy(dup->buf, string->buf, string->len);
	dup->len = string->len;
	return dup;
}
