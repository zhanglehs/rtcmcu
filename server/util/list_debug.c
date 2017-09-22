/*
 * Copyright 2006, Red Hat, Inc., Dave Jones
 * Released under the General Public License (GPL).
 *
 * This file contains the linked list implementations for
 * DEBUG_LIST.
 */

//#include <linux/module.h>
#include <assert.h>
#include "list.h"

#ifdef CONFIG_DEBUG_LIST
/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */

void
__list_add(struct list_head *newn,
           struct list_head *prev, struct list_head *next)
{
    assert(next->prev == prev);
    assert(prev->next == next);
    next->prev = newn;
    newn->next = next;
    newn->prev = prev;
    prev->next = newn;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
void
list_del(struct list_head *entry)
{
    assert(entry->prev->next == entry);
    assert(entry->next->prev == entry);
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}
#endif
