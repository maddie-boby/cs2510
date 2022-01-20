#ifndef __CLIENT_LIST_H__
#define __CLIENT_LIST_H__

struct llist_node {
    struct llist_node *prev;
    struct llist_node *next;
    void *data;
};

struct llist {
    struct llist_node *head;
    struct llist_node *tail;
};

struct llist *create_llist(void);
void free_llist(struct llist *l);
void free_llist_nodes(struct llist *l, void (*freefn)(void *));

int llist_is_empty(struct llist *l);

void llist_insert_head(struct llist *l, void *data);
void llist_insert_tail(struct llist *l, void *data);

void *llist_remove_head(struct llist *l);
void *llist_remove_tail(struct llist *l);
void *llist_remove_node(struct llist *l, struct llist_node *n);
void *llist_remove_value(struct llist *l, void *value);

void llist_for_each(struct llist *l, void (*fn)(void *, void *), void *arg);

#endif
