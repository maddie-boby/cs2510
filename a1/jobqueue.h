#ifndef __JOBQUEUE_H__
#define __JOBQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct jobqueue;

struct jobqueue *create_jobqueue(int broadcast);

void free_jobqueue(struct jobqueue * jq);

void jobqueue_enqueue(struct jobqueue * jq, void * data);

void *jobqueue_dequeue(struct jobqueue * jq);

int jobqueue_is_empty(struct jobqueue * jq);

// void jobqueue_dec(struct jobqueue * jq);

#ifdef __cplusplus
}
#endif

#endif
