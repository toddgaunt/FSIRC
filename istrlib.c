#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "istrlib.h"

// Safe add
// Checks for overflow, if it would happen 
// instead return SIZE_MAX and set errno
static inline size_t safe_add(size_t a, size_t b)
{
	if (a > SIZE_MAX - b) {
		errno = ERANGE;
		return SIZE_MAX;
	} else {
		return a + b;
	}
}

// Find the max
static inline size_t smax(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

// Compute the nearest power to num
static inline size_t next_pow(size_t base, size_t num)
{
	// Catch powers of 0, as they will break the loop below
	if (base == 0) return 0;

	// Check if the next pow will overflow
	if (num > SIZE_MAX / 2) return SIZE_MAX;
		
	while (base < num) base <<= 1 ;
	return base;
}

/*
 * desc: Ensures that string char buffer will have enough memory
 *   to hold at least len bytes. If not realloc will be called.
 */
static istring* istr_ensure_size(istring *string, size_t len)
{
	if (NULL == string) {
		errno = EINVAL;
		return NULL;
	}

	if (string->size < len) {
		string->size = next_pow(2, len);
		string->buf = realloc(string->buf, sizeof(string->buf) * (string->size));
		if (NULL == string->buf) {
			free(string);
			errno = ENOMEM;
			return NULL;
		}
	}

	return string;
}

static istring* istr_init(size_t init_size)
{
	istring *string = malloc(sizeof(*string));
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

	string->buf = NULL;
	string->size = 0;
	string->len = 0;

	// This allocates the char buffer in powers of two.
	istr_ensure_size(string, smax(init_size, 2));

	return string;
}

istring* istr_new(const istring *src) 
{
	if (NULL == src) return istr_init(0);

	istring *string = istr_init(src->len);

	if (NULL == string) {
		return NULL;
	}

	return istr_assign_bytes(string, src->buf, src->len);
}

istring* istr_new_bytes(const char *str, size_t str_len) 
{
	if (NULL == str) return istr_init(0);

	istring *string = istr_init(str_len);

	if (NULL == string) {
		return NULL;
	}

	return istr_assign_bytes(string, str, str_len);
}

istring* istr_new_cstr(const char *str) 
{
	if (NULL == str) return istr_init(0);

	size_t str_len = strlen(str) + 1;

	istring *string = istr_init(str_len);

	if (NULL == string) {
		return NULL;
	}

	return istr_assign_bytes(string, str, str_len);
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

istring* istr_assign_bytes(istring *string, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	string = istr_ensure_size(string, str_len);
	if (NULL == string) {
		return NULL;
	}

	// Don't bother memsetting the buffer, just shorten the logical length
	memcpy(string->buf, str, str_len);
	string->len = str_len;

	return string;
}

istring* istr_assign_cstr(istring *string, const char *str)
{
	if (NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	return istr_assign_bytes(string, str, strlen(str)+1);
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

istring* istr_write(istring *string, size_t index, istring *ext)
{
	return istr_write_bytes(string, index, ext->buf, ext->len);
}

istring* istr_write_bytes(istring *string, size_t index, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	size_t potential_len = safe_add(index, str_len);

	if (string->len < potential_len) {
		string->len = potential_len;
	}

	string = istr_ensure_size(string, string->len);
	if (NULL == string) {
		return NULL;
	}

	memcpy(string->buf + index, str, str_len);

	return string;
}

// TODO
istring* istr_prepend(istring *string, istring *ext)
{
	return string;
}

// TODO
istring* istr_prepend_bytes(istring *string, const char *str, size_t str_len)
{
	return string;
}

istring* istr_append(istring *dest, istring *src)
{
	if (NULL == src) {
		errno = EINVAL;
		return NULL;
	}

	return istr_append_bytes(dest, src->buf, src->len);
}

istring* istr_append_bytes(istring *string, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	// Overflow check
	string->len = safe_add(string->len, str_len);

	string = istr_ensure_size(string, string->len);
	if (NULL == string) {
		return NULL;
	}

	memcpy(string->buf + string->len, str, str_len);

	return string;
}

// TODO
istring* istr_insert(istring *dest, size_t index, istring *src)
{
	return dest;
}

// TODO
istring* istr_insert_bytes(istring *string, size_t index, const char *str, size_t str_len)
{
	if (NULL == string || NULL == str) {
		errno = EINVAL;
		return NULL;
	}

	// Index is out of bounds, don't modify the string
	if (index > string->len) {
		return string;
	}

	return string;
}
