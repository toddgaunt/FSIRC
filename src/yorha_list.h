#ifndef YORHA_LIST_H
#define YORHA_LIST_H

#include <stdbool.h>

#define LIST_DECLARE(name) struct name##_list;
#define LIST_DEFINE(name, type)                                               \
struct name##_list {                                                          \
	struct type ## _list *next;                                           \
	struct type ## _list *prev;                                           \
	type node;                                                            \
};                                                                            \
                                                                              \
static inline name##_list *                                                   \
name##_listinit(struct name ## _list *head)                                   \
{                                                                             \
	head->next = head;                                                    \
	head->prev = head;                                                    \
	return head;                                                          \
}                                                                             \
                                                                              \
static inline void                                                            \
name##_listadd(struct name##_list *head, struct name##_list *new)             \
{                                                                             \
	new->next = head;                                                     \
	new->prev = head->prev;                                               \
	new->prev->next = new;                                                \
	new->next->prev = new;                                                \
}                                                                             \
                                                                              \
static inline name##_list *                                                   \
name##_listrm(struct name##_list *head)                                       \
{                                                                             \
	head->prev->next = head->next;                                        \
	head->next->prev = head->prev;                                        \
	return head;                                                          \
}                                                                             \
                                                                              \
static inline void                                                            \
name##_listxch(struct name##_list *old, struct name##_list *new)              \
{                                                                             \
	old->next->prev = new;                                                \
	old->prev->next = new;                                                \
	new->next = old->next;                                                \
	new->prev = old->prev;                                                \
}                                                                             \
                                                                              \
static inline bool                                                            \
name##_listempty(struct name##_list *head)                                    \
{                                                                             \
    return !(head == head->next) || !(head == head->prev);                    \
}

#endif
