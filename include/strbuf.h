/* See LICENSE file for copyright and license details */
#ifndef STRBUF_H
#define STRBUF_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SB_INIT {sb_null, 0, 0}
#define SB_LIT(str) (struct strbuf const){ \
	(str), \
	sizeof(str) / sizeof((str)[0]) - 1, \
	sizeof(str) / sizeof((str)[0])}
#define SB_STATIC(str) { \
	(str), \
	sizeof(str) / sizeof((str)[0]) - 1, \
	sizeof(str) / sizeof((str)[0])}

/* Allocators used by strbuf */
#define sb_free free
#define sb_malloc malloc
#define sb_realloc realloc

extern char sb_null[1];

struct strbuf {
	char *mem;
	size_t len;
	size_t size;
};

static inline bool
sb_valid(struct strbuf *bufptr)
{
	if (!bufptr || !bufptr->mem || bufptr->size < bufptr->len)
		return false;
	return true;
}

static inline size_t
sb_avail(struct strbuf *bufptr)
{
	return bufptr->size - bufptr->len;
}

static inline void
sb_term(struct strbuf *bufptr)
{
	bufptr->mem[bufptr->len] = '\0';
}

void sb_init(struct strbuf *bufptr, size_t hint);
void sb_release(struct strbuf *bufptr);
void sb_attach(struct strbuf *bufptr, void *, size_t len, size_t size);
char *sb_detach(struct strbuf *bufptr, size_t *len, size_t *size);
bool sb_cmp(struct strbuf const *s1, struct strbuf const *s2);
size_t sb_utf8f32(void *dest, uint32_t wc);
size_t sb_utf8len(struct strbuf const *bufptr);
size_t sb_utf8n32(uint32_t wc);
void sb_cat_mem(struct strbuf *dest, void const *src, size_t n);
void sb_cat_sb(struct strbuf *dest, struct strbuf const *src);
void sb_cat_str(struct strbuf *dest, char const *src);
void sb_cat_utf8f32(struct strbuf *dest, uint32_t src);
void sb_cpy_mem(struct strbuf *dest, void const *src, size_t n);
void sb_cpy_sb(struct strbuf *dest, struct strbuf const *src);
void sb_cpy_str(struct strbuf *dest, char const *src);
void sb_dup(struct strbuf const *src);
void sb_ensure_size(struct strbuf *bufptr, size_t n);
void sb_grow(struct strbuf *bufptr, size_t n);
void sb_ins_mem(struct strbuf *dest, size_t pos, void const *src, size_t n);
void sb_ins_sb(struct strbuf *dest, size_t pos, struct strbuf const *src);
void sb_ins_str(struct strbuf *dest, size_t pos, char const *src);
void sb_ins_utf8f32(struct strbuf *dest, size_t pos, uint32_t wc);
void sb_lstrip(struct strbuf *bufptr, char const *chs);
void sb_rstrip(struct strbuf *bufptr, char const *chs);
void sb_strip(struct strbuf *bufptr, char const *chs);
void sb_trunc(struct strbuf *bufptr, size_t n);
bool sb_tok_mem(struct strbuf *dest, char **saveptr, void const *delim, size_t n);
bool sb_tok_str(struct strbuf *dest, char **saveptr, char const *delim);
bool sb_tok_sb(struct strbuf *dest, char **saveptr, struct strbuf const *delim);
void sb_slice(struct strbuf *dest, struct strbuf const *src, size_t begin,
		size_t end);

#endif
