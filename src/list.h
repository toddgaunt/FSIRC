/* See LICENSE file for copyright and license details */
#ifndef ALTERRA_LIST_H
#define ALTERRA_LIST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Retrieve a pointer to the container of the list.
 *
 * Since this list is meant to be embedded into other structs, this
 * is how you "retrieve" that entry from the list. This is done by
 * calculating the offset of the pointer to the entry relative to the
 * list struct. This must be implemented as a macro due to requiring to
 * know the name of the member of the containing struct.
 */
#define list_get(ptr, type, member) \
    ((type *)((int8_t *)(ptr) - offsetof(type, member)))

struct list {
    struct list *next;
    struct list *prev;
};

/**
 * Intialize a list entry.
 *
 * Initializes a list head by setting its next and prev pointers point to
 * itself. A circular list is created this way.
 */
void list_init(struct list *head);

/**
 * Prepend a list entry to a list entry.
 */
void list_prepend(struct list *head, struct list *new);

/**
 * Append a list entry to a list entry.
 */
void list_append(struct list *head, struct list *new);

/**
 * Remove an entry from a list.
 *
 * Remove an entry from the list it belongs to by changing the pointers
 * of the entrys adjacent to it, then making it point to itself.
 */
void list_rm(struct list *head);
void list_rm_init(struct list *head);

/**
 * Replace an entry with another entry.
 *
 * Replace an entry with another entry by changing the pointers
 * of the entrys adjacent to it to point towards the new entry. The new
 * entry will then point to the adjacent entrys. Finally the entry that
 * was replaced will point to itself.
 */
void list_replace(struct list *old, struct list *new);
void list_replace_init(struct list *old, struct list *new);

/**
 * Check if a list is empty.
 *
 * Returns whether or not the list is empty. True is returned if @head 
 * points to itself. False is returned if either pointer of @head doesn't
 * point to itself.
 */
bool list_isempty(const struct list *head);

#endif
