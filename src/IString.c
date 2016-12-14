#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "IString.h"

/*
 *
 */
IString* IString_new(const char *str, size_t len) 
{
	IString *string = malloc(sizeof(*string));
	string->buf = NULL;
	string->size = 0;
	string->len = 0;
	if (NULL != str && 0 != len) {
		IString_assign(string, str, len);
	}

	return string;
}

char* IString_free(IString *string, bool free_buf)
{
	if (string->buf) {
		if (!free_buf) {
			char *tmp = string->buf;
			free(string);
			return tmp;
		}
		free(string->buf);
	}
	free(string);

	return NULL;
}

IString* IString_assign(IString *string, const char *str, size_t len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}
	
	if (NULL == string->buf) {
		string->buf = malloc(sizeof(string->buf)*BASE_STR_LEN);
		string->size = BASE_STR_LEN;
		string->len = 0;
	}

	size_t i = 0;
	while('\0'!=str[i] || i < len) {
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

/*
 * return:
 *   0: strings are equal
 *   1: strings are not equal
 *   -1: Invalid arguments and errno set to EINVAL
 */
int IString_eq(const IString *s1, const IString *s2)
{
	if (NULL == s1, NULL == s2) {
		errno = EINVAL;
		return -1;
	}
	
	if (s1->len != s2->len) return 1;

	int i;
	for (i=0; i<s1->len; i++) {
		if (s1->buf[i] != s2->buf[i]) return 1;
	}
	return 0;
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

IString* IString_append_s(IString *string, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	// Double the string buffer if out of space
	if (string->size < (string->len + str_len + 1)) {
		string->size *= 2;
		string->buf = realloc(string->buf, sizeof(string->buf) * (string->size));
		if (!string->buf) {
			errno = ENOMEM;
			return NULL;
		}
	}

	int i;
	for (i=string->len; i<str_len;i++) {
		string->buf[i] = str[i - string->len];
	}
	string->len += str_len;

	string->buf[i] = '\0';
	return string;
}
