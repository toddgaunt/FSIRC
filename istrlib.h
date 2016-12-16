#ifndef istr_H_INCLUDED
#define istr_H_INCLUDED

#define BASE_STR_LEN 16

#include <stdlib.h>
#include <stdbool.h>

// WARNING: Do not access these fields directly outside of test code
// this API is designed to not require the internals to be messed with
// as they could potentially change in the future.
typedef struct istring {
	char *buf;     // Character buffer.
	size_t size;   // Size of character buffer.
	size_t len; // Amount of characters in the buffer.
} istring;


/* istr_new
 * return -> istring*
 *   success: if source is not NULL, duplicate *source exactly and return it,
 *     otherwise return an initialized string object.
 *   memory error: NULL and errno = ENOMEM
 */
istring* istr_new(const istring *source);

istring* istr_new_bytes(const char *str, size_t len);

istring* istr_new_cstr(const char *str);

/* istr_free
 * return: char*
 *   success: the string object's char buffer
 *   bad args: errno = EINVAL and NULL;
 */
char* istr_free(istring *string, bool free_buf);

/* istr_str
 * desc: Returns a pointer to the istr char buffer.
 *   Use this for interoperability with <string.h> functions.
 *   Please be careful, the buffer might be realloc'd by
 *   any of the non-const istr_funcs and you'll have to call
 *   this function again to avoid pointing to
 *   memory you shouldn't be pointing to.
 *
 *   TL;DR - Use the pointer returned by this functions
 *   as soon as possible before calling other istr_funcs on it
 * return: char*
 *   success: The string object's char buffer
 *   bad args: errno = EINVAL and return NULL.
 */
char* istr_str(const istring *string);

/* istr_len
 * return: size_t
 *   success: The string object's length.
 *   bad args: 0 and errno = EINVAL.
 */
size_t istr_len(const istring *string);

/*
 * TODO
 *
 */
size_t istr_size(const istring *string);

/* istr_eq
 * return -> int
 *   equal: 0
 *   not equal: 1
 *   bad args: -1 and errno = EINVAL
 */
int istr_eq(const istring *s1, const istring *s2);

/* istr_copy
 * return -> istring*
 * TODO
 *   
 */
istring* istr_assign_bytes(istring *string, const char *str, size_t len);

/*
 * TODO
 */
istring* istr_assign_cstr(istring *string, const char *str);

/*
 * TODO
 */
istring* istr_write(istring *string, size_t index, istring *ext);

/*
 * TODO
 */
istring* istr_write_bytes(istring *string, size_t index, const char *str, size_t str_len);

/*
 *
 * TODO
 */
istring* istr_prepend(istring *string, istring *ext);

/*
 *
 * TODO
 */
istring* istr_prepend_bytes(istring *string, const char *str, size_t str_len);

/*
 *TODO
 *
 */
istring* istr_append(istring *string, istring *extension);

/* istr_append
 * return -> istring*
 *   success: original string object with *str appended to char buffer.
 *   bad args: NULL & errno=EINVAL
 *   memory error: NULL & errno=ENOMEM
 */
istring* istr_append_bytes(istring *string, const char *str, size_t str_len);

#endif
