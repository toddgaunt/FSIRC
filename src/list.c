/* See LICENSE file for copyright and license details */
#include <stdbool.h>
#include "list.h"

// For documentation on any of these functions, please see list.h.

/**
 * Connect list structures together.
 *
 * This is for internal use only. The functions exposed from this
 * module are list_append and list_prepend
 */
static inline void list_link(
        struct list *new,
        struct list *next,
        struct list *prev
        )
{
    new->next = next;
    new->prev = prev;
    next->prev = new;
    prev->next = new;
}

void list_init(struct list *head)
{
    head->next = head;
    head->prev = head;
}

void list_prepend(struct list *head, struct list *new)
{
    list_link(new, head, head->prev);
}

void list_append(struct list *head, struct list *new)
{
    list_link(new, head->next, head);
}

void list_rm(struct list *head)
{
    head->next->prev = head->prev;
    head->prev->next = head->next;
}

void list_rm_init(struct list *head)
{
    list_rm(head);
    list_init(head);
}

void list_replace(struct list *old, struct list *new)
{
    old->next->prev = new;
    old->prev->next = new;
    new->next = old->next;
    new->prev = old->prev;
}

void list_replace_init(struct list *old, struct list *new)
{
    list_replace(old, new);
    list_init(old);
}

bool list_isempty(const struct list *head)
{
    return (head == head->next) && (head == head->prev);
}

// End of file
