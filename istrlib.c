#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "istrlib.h"

static istring* istr_init()
{
	istring *string = malloc(sizeof(*string));
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

	string->buf = NULL;
	string->size = 0;
	string->len = 0;

	return string;
}

static istring* istr_dup(const istring *string) 
{
	if (NULL == string) return NULL;

	istring *dup = istr_init();
	if (NULL == dup) {
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

istring* istr_new(istring *source) 
{
	if (NULL != source) {
		return istr_dup(source);
	}

	return istr_init();
}

istring* istr_new_bytes(const char *str, size_t len) 
{
	istring *string = istr_init();

	if (NULL != string && NULL != str && 0 != len) {
		istr_append_bytes(string, str, len);
	}

	return string;
}

istring* istr_new_cstr(const char *str) 
{
	istring *string = istr_init();

	if (NULL != string && NULL != str) {
		// 1 extra for the '\0'
		size_t len = strlen(str) + 1;
		istr_append_bytes(string, str, len);
	}

	return string;
}

char* istr_free(istring *string, bool free_buf)
{
	if (string->buf) {
		if (!free_buf) {
			char *tmp = string->buf;
			free(string);
			return tmp;
		}
		free(string->buf);
	}
	if (string) free(string);

	return NULL;
}

char* istr_str(const istring *string)
{
	if (NULL == string) {
		errno = EINVAL;
		return NULL;
	}

	return string->buf;
}

size_t istr_len(const istring *string)
{
	if (NULL == string) {
		errno = EINVAL;
		return 0;
	}
	return string->len;
}

size_t istr_size(const istring *string)
{
	if (NULL == string) {
		errno = EINVAL;
		return 0;
	}
	return string->size;
}

istring* istr_assign_bytes(istring *string, const char *str, size_t len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}
	
	if (NULL == string->buf) {
		string->buf = malloc(sizeof(string->buf)*BASE_STR_LEN);
		if (!string->buf) {
			errno = ENOMEM;
			return NULL;
		}
		string->size = BASE_STR_LEN;
		string->len = 0;
	}

	size_t i = 0;
	while(i < len) {
		string->buf[i] = str[i];
		i++;
		if (string->size < i) {
			// Double the allocated memory
			string->size *= 2;
			string->buf = realloc(string->buf, \
					sizeof(string->buf) * (string->size));
			if (!string->buf) {
				errno = ENOMEM;
				return NULL;
			}
		}
	}
	string->len = i;
	return string;
}

istring* istr_assign_cstr(istring *string, const char *str)
{
	if (NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	size_t len = strlen(str) + 1;

	return istr_assign_bytes(string, str, len);
}

int istr_eq(const istring *s1, const istring *s2)
{
	if (NULL == s1 || NULL == s2) {
		errno = EINVAL;
		return -1;
	}
	
	if (s1->len != s2->len) return 1;

	for (size_t i=0; i<s1->len; i++) {
		if (s1->buf[i] != s2->buf[i]) return 1;
	}

	return 0;
}

//TODO
istring* istr_append(istring *string, istring *extension)
{
	return NULL;
}

istring* istr_append_bytes(istring *string, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	// Increase the size of the char buffer
	if (string->size < (string->len + str_len)) {
		string->size += str_len;
		string->buf = realloc(string->buf, \
			sizeof(string->buf) * (string->size));
		if (!string->buf) {
			errno = ENOMEM;
			return NULL;
		}
	}

	size_t i;
	for (i=string->len; i<str_len;i++) {
		string->buf[i] = str[i - string->len];
	}
	string->len += str_len;

	return string;
}
