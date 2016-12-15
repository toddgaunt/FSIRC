#ifndef istr_H_INCLUDED
#define istr_H_INCLUDED

#define BASE_STR_LEN 16

#include <stdlib.h>
#include <stdbool.h>

// WARNING: Do not access these fields directly outside of test code
// this API is designed to not require the internals to be messed with
// as they could potentially change in the future
typedef struct istring {
	char *buf;     // character buffer
	size_t size;   // size of character buffer
	size_t len; // amount of characters until '\0'
} istring;

/* istr_str
 * return: char*
 *   success: the string object's char buffer
 *   bad args: errno = EINVAL and NULL;
 */
istring* istr_new(const char *str, size_t len);

/* istr_str
 * return: char*
 *   success: the string object's char buffer
 *   bad args: errno = EINVAL and NULL;
 */
char* istr_free(istring *string, bool free_buf);

/* istr_str
 * return: char*
 *   success: the string object's char buffer
 *   bad args: errno = EINVAL and NULL;
 */
char* istr_str(istring *string);

/* istr_len
 * return: size_t
 *   success: the string object's length value
 *   bad args: 0 and errno = EINVAL
 */
size_t istr_len(istring *string);

/* istr_assign
 * return -> istring*
 *   
 */
istring* istr_assign(istring *string, const char *str, size_t len);

/* istr_eq
 * return -> int
 *   equal: 0
 *   not equal: 1
 *   bad args: -1 and errno = EINVAL
 */
int istr_eq(const istring *s1, const istring *s2);

/* istr_dup
 * return -> istring*
 *   success: duplicate *string exactly and return it
 *   memory error: NULL and errno = ENOMEM
 */
istring* istr_dup(const istring *string);

/* istr_append
 * return -> istring*
 *   success: original string object with *str appended to char buffer
 *   bad args: NULL & errno=EINVAL
 *   memory error: NULL & errno=ENOMEM
 */
istring* istr_append(istring *string, const char *str, size_t str_len);

#endif
