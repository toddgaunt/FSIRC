#ifndef YORHA_LIST_H
#define YORHA_LIST_H
#include <stdbool.h>
#define DECLARE_LIST(type) struct type ## _list;
#define DEFINE_LIST(type)\
typedef struct type ## _list {\
	struct type ## _list *next;\
	struct type ## _list *prev;\
	type node;\
} type ## _list;\
type ## _list *type ## _listinit(struct type ## _list *head)\
{\
	head->next = head;\
	head->prev = head;\
	return head;\
}\
void type ## _listadd(struct type ## _list *head, struct type ## _list *new)\
{\
	new->next = head;\
	new->prev = head->prev;\
	new->prev->next = new;\
	new->next->prev = new;\
}\
type ## _list *type ## _listrm(struct type ## _list *head)\
{\
	head->prev->next = head->next;\
	head->next->prev = head->prev;\
	return head;\
}\
void type ## _listxch(struct type ## _list *old, struct type ## _list *new)\
{\
	old->next->prev = new;\
	old->prev->next = new;\
	new->next = old->next;\
	new->prev = old->prev;\
}\
bool type ## _listempty(struct type ## _list *head)\
{\
    return (head == head->next) && (head == head->prev);\
}
#endif
