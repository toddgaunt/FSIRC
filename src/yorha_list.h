#ifndef YORHA_LIST_H
#define YORHA_LIST_H

#include <stdbool.h>

#define LIST_DECLARE(NAME) struct NAME;

#define LIST_DEFINE(NAME, TYPE)                                               \
struct NAME {                                                                 \
	struct NAME *next;                                                    \
	struct NAME *prev;                                                    \
	TYPE node;                                                            \
};                                                                            \
                                                                              \
static inline struct NAME *                                                   \
NAME##_init(struct NAME *lp)                                                  \
{                                                                             \
	lp->next = lp;                                                        \
	lp->prev = lp;                                                        \
	return lp;                                                            \
}                                                                             \
                                                                              \
static inline void                                                            \
NAME##_append(struct NAME *head, struct NAME *new)                            \
{                                                                             \
	new->next = head->next;                                               \
	new->prev = head;                                                     \
	new->prev->next = new;                                                \
	new->next->prev = new;                                                \
}                                                                             \
                                                                              \
static inline void                                                            \
NAME##_prepend(struct NAME *head, struct NAME *new)                           \
{                                                                             \
	new->next = head;                                                     \
	new->prev = head->prev;                                               \
	new->prev->next = new;                                                \
	new->next->prev = new;                                                \
}                                                                             \
                                                                              \
static inline struct NAME *                                                   \
NAME##_rm(struct NAME *lp)                                                    \
{                                                                             \
	lp->prev->next = lp->next;                                            \
	lp->next->prev = lp->prev;                                            \
	return lp;                                                            \
}                                                                             \
                                                                              \
static inline void                                                            \
NAME##_xch(struct NAME *old, struct NAME *new)                                \
{                                                                             \
	old->next->prev = new;                                                \
	old->prev->next = new;                                                \
	new->next = old->next;                                                \
	new->prev = old->prev;                                                \
}                                                                             \
                                                                              \
static inline bool                                                            \
NAME##_empty(struct NAME *lp)                                                 \
{                                                                             \
    return !(lp == lp->next) || !(lp == lp->prev);                            \
}

#endif
