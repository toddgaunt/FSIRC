/* See LICENSE file for copyright and license details */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

char sb_null[1] = "";

/* Return the smallest number */
static inline size_t
internal_min(size_t a, size_t b)
{
	return a > b ? b : a;
}

/* Compute the next power of two that is greater than or equal to `n` */
static inline size_t
internal_npow2(size_t n)
{
	size_t next = 1;

	if (n >= SIZE_MAX / 2) {
		next = SIZE_MAX;
	} else {
		while (next < n)
			next <<= 1;
	}
	return next;
}

/*
 * A capped addition function for size_t types. Any overflow rounds off to the
 * value SIZE_MAX.
 */
static inline bool
internal_add_overflows(size_t a, size_t b)
{
	return a > SIZE_MAX - b ? true : false;
}

/* Initialize a strbuf */
void
sb_init(struct strbuf *bufptr, size_t hint)
{
	bufptr->mem = sb_null;
	bufptr->len = 0;
	bufptr->size = 0;
	if (hint)
		sb_grow(bufptr, hint);
}

void
sb_release(struct strbuf *bufptr)
{
	if (sb_null != bufptr->mem)
		free(bufptr->mem);
	sb_init(bufptr, 0);
}

/* Concatenate to a strbuf */
void
sb_cat_mem(struct strbuf *dest, void const *src, size_t n)
{
	sb_grow(dest, n);
	memcpy(dest->mem + dest->len, src, n);
	dest->len += n;
	sb_term(dest);
}

void
sb_cat_str(struct strbuf *dest, char const *src)
{
	sb_cat_mem(dest, src, strlen(src));
}

void
sb_cat_sb(struct strbuf *dest, struct strbuf const *src)
{
	sb_cat_mem(dest, src->mem, src->len);
}

/* Copy to a strbuf */
void
sb_cpy_mem(struct strbuf *dest, void const *src, size_t n)
{
	sb_ensure_size(dest, n);
	memcpy(dest->mem, src, n);
	dest->len = n;
	sb_term(dest);
}

void
sb_cpy_str(struct strbuf *dest, char const *src)
{
	sb_cpy_mem(dest, src, strlen(src));
}

void
sb_cpy_sb(struct strbuf *dest, struct strbuf const *src)
{
	sb_cpy_mem(dest, src->mem, src->len);
}

/* Grow a strbuf */
void
sb_grow(struct strbuf *bufptr, size_t hint)
{
	/* Don't touch the buffer if the hint is 0 */
	if (0 == hint)
		return;
	if (internal_add_overflows(hint, 1)
	|| internal_add_overflows(bufptr->len, hint + 1)) {
		/* Attempting to use far too much memory */
		exit(EXIT_FAILURE);
	} else {
		hint = internal_npow2(bufptr->len + hint + 1);
	}
	if (0 == bufptr->size) {
		bufptr->mem = sb_malloc(hint);
	} else {
		bufptr->mem = sb_realloc(bufptr->mem, hint);
	}
	bufptr->size = hint;
}

void
sb_ensure_size(struct strbuf *bufptr, size_t hint)
{
	/* No resize needed if bufptr->size >= n */
	if (bufptr->size >= hint)
		return;
	if (internal_add_overflows(hint, 1)) {
		/* Attempting to use far too much memory */
		exit(EXIT_FAILURE);
	} else {
		hint = internal_npow2(hint + 1);
	}
	if (0 == bufptr->size) {
		bufptr->mem = sb_malloc(hint);
	} else {
		bufptr->mem = sb_realloc(bufptr->mem, hint);
	}
	bufptr->size = hint;
}

/* Compare strbufs */
bool
sb_cmp(struct strbuf const *a, struct strbuf const *b)
{
	size_t i;

	/* Must be the same length */
	if (a->len != b->len)
		return false;
	/* Compare chunks of register size for speed */
	const size_t chunks = a->len / sizeof(i);
	for (i = 0; i < chunks; ++i) {
		if (*((size_t *)a->mem + i) != *((size_t *)b->mem + i))
			return false;
	}
	/* Compare the leftover bytes */
	const size_t n = chunks * sizeof(i);
	for (i = 0; i < a->len % sizeof(i); ++i) {
		if (a->mem[n + i] != b->mem[n + i])
			return false;
	}
	return true;
}

/* Insert characters at an index of a strbuf */
void
sb_ins_mem(struct strbuf *dest, size_t pos, void const *src, size_t n)
{
	sb_grow(dest, n);
	/* Create some space if inserting before the end of the buffer. */
	if (pos < dest->len)
		memmove(dest->mem + pos + n, dest->mem + pos, dest->len - pos);
	memmove(dest->mem + pos, src, n);
	dest->len += n;
	sb_term(dest);
}

void
sb_ins_cstr(struct strbuf *dest, size_t pos, char const *src)
{
	sb_ins_mem(dest, pos, src, strlen(src));
}

void
sb_ins_utf8f32(struct strbuf *dest, size_t pos, uint32_t src)
{
	char uni8[4];
	size_t n = sb_utf8f32(uni8, src);

	sb_ins_mem(dest, pos, uni8, n);
}

void
sb_ins_sb(struct strbuf *dest, size_t pos, struct strbuf const *src)
{
	sb_ins_mem(dest, pos, src->mem, src->len);
}

/* Strip characters from a strbuf */
void
ustr_lstrip_mem(struct strbuf *bufptr, void const *chs, size_t len)
{
	size_t removed = 0;
	char *begin = bufptr->mem;
	char *end = bufptr->mem + bufptr->len;

	while (begin != end && memchr(chs, *begin, len)) {
		++removed;
		++begin;
	}
	if (begin != bufptr->mem)
		memmove(bufptr->mem, begin, bufptr->len - removed);
	bufptr->len -= removed;
}

void
ustr_rstrip_mem(struct strbuf *bufptr, void const *chs, size_t len)
{
	size_t removed = 0;
	char *begin = bufptr->mem + bufptr->len - 1;
	char *end = bufptr->mem;

	if (0 == bufptr->len)
		return;
	while (begin != end && memchr(chs, *begin, len)) {
		++removed;
		--begin;
	}
	bufptr->len -= removed;
}

void
ustr_strip_mem(struct strbuf *bufptr, void const *chs, size_t len)
{
	ustr_lstrip_mem(bufptr, chs, len);
	ustr_rstrip_mem(bufptr, chs, len);
}

void
ustr_lstrip_str(struct strbuf *bufptr, char const *chs)
{
	ustr_lstrip_mem(bufptr, chs, strlen(chs));
}

void
ustr_rstrip_str(struct strbuf *bufptr, char const *chs)
{
	ustr_rstrip_mem(bufptr, chs, strlen(chs));
}

void
ustr_strip_str(struct strbuf *bufptr, char const *chs)
{
	ustr_strip_mem(bufptr, chs, strlen(chs));
}

/* Truncate to a specified length */
void
sb_trunc(struct strbuf *bufptr, size_t n)
{
	if (n >= bufptr->len) {
		bufptr->len = 0;
	} else {
		bufptr->len -= n;
	}
	sb_term(bufptr);
}

/* Split up a strbuf into tokens with a delimiter */
bool
sb_tok_mem(struct strbuf *dest, char **saveptr, void const *delim, size_t n)
{
	size_t n;
	size_t len;
	char *pos = *savptr;

	for (; *cursor; ++cursor) {
		for (j = 0; j < n; ++j) {
			if (*cursor != ((char *)delim)[j]) {
				break;
			}
		}
		if (j == n) {
			// Create the token.
			sb_init(dest);
			sb_cat_str(dest, cursor);
			// Save the reference
			*saveptr = cursor;
			return true;
		}
	}
	return false;
}

bool
sb_tok_str(struct strbuf *dest, char **saveptr, char const *delim)
{
	return ustr_tok_mem(dest, pos, delim, strlen(delim));
}

bool
sb_tok_sb(struct strbuf *dest, char **saveptr, struct strbuf const *delim)
{
	return ustr_tok_mem(dest, pos, delim->mem, delim->len);
}

/* Create a new strbuf as a slice of another strbuf */
void
sb_slice(struct strbuf *dest, struct strbuf const *src, size_t begin, size_t end)
{
	sb_init(dest);
	sb_cat_mem(dest, src.mem + begin, end - begin);
}

# End of file
