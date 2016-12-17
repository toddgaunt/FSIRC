#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "istrlib.h"

/* safe_add
 * desc: Safely adds together two size_t values while preventing 
 *   integer overflow. If integer overflow would occur, instead
 *   the maximum possible size is returned instead and errno is 
 *   set to ERANGE.
 * return -> size_t
 *   success: Sum of the two arguments
 *   range error: SIZE_MAX and errno = ERANGE
 */
static inline size_t safe_add(size_t a, size_t b)
{
	if (a > SIZE_MAX - b) {
		errno = ERANGE;
		return SIZE_MAX;
	} else {
		return a + b;
	}
}

/* smax
 * desc: simple inline function that returns the largest
 *   input. Used instead of a macro for safety
 * return -> size_t
 *   success: largest value between a and b
 */
static inline size_t smax(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

/* nearest_pow
 * desc: Find the nearest power of two to the value of num.
 *   if the next power of two would overflow, then
 *   SIZE_MAX is returned instead with errno set appropriately.
 * return -> size_t
 *   success: The closest power of two larger than num
 *   range error: SIZE_MAX and errno = ERANGE
 */
static inline size_t nearest_pow(size_t base, size_t num)
{
	// Catch powers of 0, as they will break the loop below
	if (base == 0) return 0;

	// Check if the next pow will overflow
	if (num > SIZE_MAX / 2) {
		errno = ERANGE;
		return SIZE_MAX;
	}
		
	while (base < num) base <<= 1 ;
	return base;
}

/* istr_ensure_size
 * desc: A Helper function that guarentees to the caller 
 *   that, if memory can be allocated successfully, that @string's
 *   char buffer will be able to hold at least @len bytes
 * return -> istring*
 *   success: The original string object
 *   bad args: NULL and errno = EINVAL
 *   memory error: NULL and errno = ENOMEM
 */
static istring* istr_ensure_size(istring *string, size_t len)
{
	if (NULL == string) {
		errno = EINVAL;
		return NULL;
	}

	if (string->size < len) {
		string->size = nearest_pow(2, len);
		string->buf = realloc(string->buf, sizeof(string->buf) * (string->size));
		if (NULL == string->buf) {
			free(string);
			errno = ENOMEM;
			return NULL;
		}
	}

	return string;
}

/* istr_init
 * desc: A Helper function that allocates memory for an istring
 *   and initializes all of it's fields.
 * return -> istring*
 *   success: The pointer to a newly allocated istring
 *   failure: NULL and errno = ENOMEM
 */
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
	string = istr_ensure_size(string, smax(init_size, 2));
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

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

istring* istr_new_bytes(const char *str, size_t bytes_len) 
{
	if (NULL == str) return istr_init(0);

	istring *string = istr_init(bytes_len);

	if (NULL == string) {
		return NULL;
	}

	return istr_assign_bytes(string, str, bytes_len);
}

istring* istr_new_cstr(const char *cstr) 
{
	if (NULL == cstr) return istr_init(0);

	size_t bytes_len = strlen(cstr) + 1;

	istring *string = istr_init(bytes_len);

	if (NULL == string) {
		return NULL;
	}

	return istr_assign_bytes(string, cstr, bytes_len);
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

istring* istr_assign_bytes(istring *string, const char *bytes, size_t bytes_len)
{
	if (NULL == string || NULL == bytes) {
		errno = EINVAL;
		return NULL;
	}

	string = istr_ensure_size(string, bytes_len);
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

	// Don't bother memsetting the buffer, just shorten the logical length
	memcpy(string->buf, bytes, bytes_len);
	string->len = bytes_len;

	return string;
}

istring* istr_assign_cstr(istring *string, const char *cstr)
{
	if (NULL == cstr) {
		errno = EINVAL;
		return NULL;
	}

	return istr_assign_bytes(string, cstr, strlen(cstr)+1);
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

istring* istr_write_bytes(istring *string, size_t index, const char *bytes, size_t bytes_len)
{
	if (NULL == string || NULL == bytes) {
		errno = EINVAL;
		return NULL;
	}

	size_t potential_len = safe_add(index, bytes_len);

	if (string->len < potential_len) {
		string->len = potential_len;
	}

	string = istr_ensure_size(string, string->len);
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

	memcpy(string->buf + index, bytes, bytes_len);

	return string;
}

istring* istr_prepend(istring *dest, istring *src)
{
	if (NULL == dest || NULL == src) {
		errno = EINVAL;
		return NULL;
	}
	
	return istr_prepend_bytes(dest, src->buf, src->len);
}

istring* istr_prepend_bytes(istring *string, const char *bytes, size_t bytes_len)
{
	return istr_insert_bytes(string, 0, bytes, bytes_len);
}

istring* istr_append(istring *dest, istring *src)
{
	if (NULL == src) {
		errno = EINVAL;
		return NULL;
	}

	return istr_append_bytes(dest, src->buf, src->len);
}

istring* istr_append_bytes(istring *string, const char *bytes, size_t bytes_len)
{
	if (NULL == string || NULL == bytes) {
		errno = EINVAL;
		return NULL;
	}

	return istr_insert_bytes(string, string->len, bytes, bytes_len);
}

istring* istr_insert(istring *dest, size_t index, istring *src)
{
	if (NULL == dest || NULL == src) {
		errno = EINVAL;
		return NULL;
	}

	return istr_insert_bytes(dest, index, src->buf, src->len);
}

istring* istr_insert_bytes(istring *string, size_t index, const char *bytes, size_t bytes_len)
{
	if (NULL == string || NULL == bytes) {
		errno = EINVAL;
		return NULL;
	}

	// Overflow check
	size_t total_len = safe_add(string->len, bytes_len);

	string = istr_ensure_size(string, total_len);
	if (NULL == string) {
		errno = ENOMEM;
		return NULL;
	}

	if (index < string->len) {
		// Create some space for the str to be inserted
		memmove(string->buf + index + bytes_len, \
		        string->buf + index, \
		        string->len - index);
	} else if (index > string->len) {
		// Initialize the space between original data and newly inserted string
		memset(string->buf + string->len, '\0', index - string->len);
	}

	memcpy(string->buf + index, bytes, bytes_len);
	string->len = total_len;

	return string;
}
