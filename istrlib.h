#ifndef istr_H_INCLUDED
#define istr_H_INCLUDED

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
 * desc: Construct a new empty istring or copy an existing istring.
 * return -> istring*
 *   success: if @src is not NULL, duplicate @src exactly and return the 
 *     duplicate. Otherwise return an initialized (but empty) string object.
 *   memory error: NULL and errno = ENOMEM
 */
istring* istr_new(const istring *src);

/* istr_new_bytes
 * desc: Construct a new istring from a string of bytes (Does not need to be
 *   null terminated!).
 * return -> istring*
 *   success: if @bytes is not NULL and bytes_len is not 0, 
 *     duplicate @src exactly and return the duplicate. Otherwise return an 
 *     initialized (but empty) string object.
 *   memory error: NULL and errno = ENOMEM
 */
istring* istr_new_bytes(const char *bytes, size_t bytes_len);

/* istr_new_bytes
 * desc: Construct a new istring from a traditional C string.
 * return -> istring*
 *   success: if @bytes is not NULL and bytes_len is not 0, 
 *     duplicate @src exactly and return the duplicate. Otherwise return an 
 *     initialized (but empty) string object.
 *   memory error: NULL and errno = ENOMEM
 */
istring* istr_new_cstr(const char *cstr);

/* istr_free
 * desc: Handles all memory deallocation of an istring object,
 *   please use this instead of manually freeing.
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

/* istr_size
 * desc: Return the amount of bytes in @string's char buffer
 * return -> size_t
 *   success: the amount of bytes in @string's char buffer
 *   bad args: 0 and errno = EINVAL
 */
size_t istr_len(const istring *string);

/* istr_size
 * desc: Return the size of @string's char buffer
 * return -> size_t
 *   success: the size of the char buffer
 *   bad args: 0 and errno = EINVAL
 */
size_t istr_size(const istring *string);

/* istr_eq
 * desc: Check if two istring objects are equivalent
 * return -> int
 *   success: 0 if equal, 1 if not equal
 *   bad args: -1 and errno = EINVAL
 */
int istr_eq(const istring *s1, const istring *s2);

/* istr_assign_bytes
 * desc: Reassign's @string's char buffer to @bytes_len amount of @bytes while
 *   resizing if necessary.
 * @index while resizing if necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_assign_bytes(istring *string, const char *bytes, size_t bytes_len);

/* istr_assign_cstr
 * desc: Reassign's @string's char buffer to @cstr while resizing if necessary.
 * @index while resizing if necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_assign_cstr(istring *string, const char *cstr);

/* istr_write
 * desc: Overwrites @dest's char buffer with @src's char buffer at
 * @index while resizing if necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_write(istring *dest, size_t index, istring *src);

/* istr_write_bytes
 * desc: Overwrites @string's char buffer with @bytes by @bytes_len amount 
 * at @index while resizing if necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_write_bytes(istring *string, size_t index, const char *bytes, size_t bytes_len);

/* istr_prepend
 * desc: Prepends a copy @src's char buffer into @dest's char buffer while 
 *   resizing as necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_prepend(istring *dest, istring *src);

/* istr_prepend_bytes
 * desc: Prepends @bytes_len amount from @bytes into @string while
 *   resizing as necessary.
 * return -> istring*
 *   success: original string object
 *   bad args: NULL & errno = EINVAL
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_prepend_bytes(istring *string, const char *bytes, size_t bytes_len);

/* istr_append
 * desc: Appends a copy @src's char buffer into @dest's char buffer while 
 *   resizing as necessary.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_append(istring *dest, istring *src);

/* istr_append_bytes
 * desc: Appends @bytes_len amount from @bytes into @string.
 * return -> istring*
 *   success: original string object
 *   bad args: NULL & errno = EINVAL
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_append_bytes(istring *string, const char *bytes, size_t bytes_len);

/* istr_insert
 * desc: Inserts a copy of @src's char buffer into @dest's char buffer
 *   at @index and resizes accordingly
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_insert(istring *dest, size_t index, istring *src);

/* istr_insert_bytes
 * desc: Inserts @bytes_len amount from @bytes into a 
 *   0-indexed position at @index in @string. Does not overwrite any
 *   characters already in the string.
 * return -> istring*
 *   success: the original string object
 *   bad args: NULL & errno=EINVAL 
 *   memory error: NULL & errno = ENOMEM
 */
istring* istr_insert_bytes(istring *string, size_t index, const char *bytes, size_t bytes_len);

#endif
