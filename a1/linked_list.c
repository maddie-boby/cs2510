#include <stdlib.h>

#include "linked_list.h"

static struct llist_node *create_llist_node(void *data)
{
    struct llist_node *n = NULL;
    n = calloc(1, sizeof(*n));

    n->data = data;
    n->prev = NULL;
    n->next = NULL;

    return n;
}

static void free_llist_node(struct llist_node *n)
{
    free(n);
}

struct llist *create_llist(void)
{
    struct llist *l = NULL;

    l = calloc(1, sizeof(*l));
    l->head = NULL;
    l->tail = NULL;
    return l;
}

void free_llist(struct llist *l)
{
    struct llist_node *cursor = NULL;
    struct llist_node *tmp = NULL;

    cursor = l->head;
    while (cursor != NULL) {
        tmp = cursor->next;
        free_llist_node(cursor);
        cursor = tmp;
    }
    free(l);
}

void free_llist_nodes(struct llist *l, void (*freefn)(void *))
{
    struct llist_node *cursor = NULL;
    struct llist_node *tmp = NULL;

    cursor = l->head;
    while (cursor != NULL) {
        tmp = cursor->next;
        freefn(cursor->data);
        free_llist_node(cursor);
        cursor = tmp;
    }
    free(l);
}

int llist_is_empty(struct llist *l)
{
    return (l->head == NULL && l->tail == NULL);
}


void llist_insert_head(struct llist *l, void *data)
{
    struct llist_node *n = NULL;
    n = create_llist_node(data);
    if (l->head == NULL) {
        l->head = n;
        l->tail = n;
    } else {
        l->head->prev = n;
        n->next = l->head;
        l->head = n;
    }
}
void llist_insert_tail(struct llist *l, void *data)
{
    struct llist_node *n = NULL;
    n = create_llist_node(data);
    if (l->tail == NULL) {
        l->head = n;
        l->tail = n;
    } else {
        l->tail->next = n;
        n->prev = l->tail;
        l->tail = n;
    }
}

void *llist_remove_head(struct llist *l)
{
    void *data = NULL;
    struct llist_node *tmp;

    tmp = l->head;
    data = tmp->data;

    if (l->head == l->tail) {
        l->head = NULL;
        l->tail = NULL;

    } else {
        l->head = tmp->next;
        l->head->prev = NULL;
    }
    free_llist_node(tmp);
    return data;
}

void *llist_remove_tail(struct llist *l)
{
    void *data = NULL;
    struct llist_node *tmp;

    tmp = l->tail;
    data = tmp->data;
    if (l->head == l->tail) {
        l->head = NULL;
        l->tail = NULL;
    } else {
        l->tail = tmp->prev;
        l->tail->next = NULL;
    }
    free_llist_node(tmp);
    return data;
}

void *llist_remove_node(struct llist *l, struct llist_node *n)
{
    void *data = NULL;

    if (n == l->head) {
        return llist_remove_head(l);
    } else if (n == l->tail) {
        return llist_remove_tail(l);
    }

    data = n->data;
    n->prev->next = n->next;
    n->next->prev = n->prev;

    free(n);

    return data;
}

void *llist_remove_value(struct llist *l, void *value)
{
    struct llist_node *cursor = NULL;
    cursor = l->head;
    while (cursor != NULL) {
        if (cursor->data == value) {
            return llist_remove_node(l, cursor);
        }
        cursor = cursor->next;
    }
    return NULL;
}


void llist_for_each(struct llist *l, void (*fn)(void *, void *), void *arg)
{
    struct llist_node *cursor = NULL;

    cursor = l->head;

    while (cursor != NULL) {
        fn(cursor->data, arg);
        cursor = cursor->next;
    }
}

