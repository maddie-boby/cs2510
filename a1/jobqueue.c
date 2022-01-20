#include <stdlib.h>

#include <pthread.h>

#include "jobqueue.h"

struct jobqueue_node {
    struct jobqueue_node *prev;
    struct jobqueue_node *next;
    void *data;
};

struct jobqueue {
    struct jobqueue_node *head;
    struct jobqueue_node *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int broadcast;
};

static struct jobqueue_node *__create_node(void *data)
{
    struct jobqueue_node *node = NULL;

    node = malloc(sizeof(*node));
    node->prev = NULL;
    node->next = NULL;
    node->data = data;

    return node;
}

static void __free_node(struct jobqueue_node *node)
{
    free(node);
}

struct jobqueue *create_jobqueue(int broadcast)
{
    struct jobqueue *jq = NULL;

    jq = malloc(sizeof(*jq));

    jq->broadcast = broadcast;
    jq->head = NULL;
    jq->tail = NULL;
    pthread_mutex_init(&jq->mutex, NULL);
    pthread_cond_init(&jq->cond, NULL);

    return jq;
}

void free_jobqueue(struct jobqueue *jq)
{
    struct jobqueue_node *cur = NULL;

    cur = jq->head;
    while (cur != NULL) {
        struct jobqueue_node *tmp = cur->next;
        __free_node(cur);
        cur = tmp;
    }

    pthread_cond_destroy(&jq->cond);
    pthread_mutex_destroy(&jq->mutex);

    free(jq);
}

void jobqueue_enqueue(struct jobqueue *jq, void *data)
{
    struct jobqueue_node *node = NULL;

    node = __create_node(data);

    pthread_mutex_lock(&jq->mutex);
    {
        // jq->active++;
        if (jq->head != NULL) {
            jq->tail->next = node;
            node->next = NULL;
            node->prev = jq->tail;
            jq->tail = node;
        } else {
            jq->head = node;
            jq->tail = node;
        }
        if (jq->broadcast) {
            pthread_cond_broadcast(&jq->cond);
        } else {
            pthread_cond_signal(&jq->cond);
        }
    }
    pthread_mutex_unlock(&jq->mutex);
}

void *jobqueue_dequeue(struct jobqueue *jq)
{
    struct jobqueue_node *tmp = NULL;

    void *val = NULL;
    // int ret = 0;

    pthread_mutex_lock(&jq->mutex);
    {
        while ((jobqueue_is_empty(jq))) {
            pthread_cond_wait(&jq->cond, &jq->mutex);
        }
        tmp = jq->head;
        val = tmp->data;
        jq->head = tmp->next;
        __free_node(tmp);

    }
    pthread_mutex_unlock(&jq->mutex);

    return val;
}

int jobqueue_is_empty(struct jobqueue *jq)
{
    return (jq->head == NULL);
}
