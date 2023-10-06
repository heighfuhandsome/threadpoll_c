#ifndef THREADPOLL_HEAD
#define THREADPOLL_HEAD
#include <pthread.h>
typedef unsigned int uint;

/* tasks in task queue */
typedef struct task_s task_t;
typedef struct threadpoll_s threadpoll_t;

threadpoll_t *threadpoll_get(uint queSize,int max,int min);

int threadpoll_addTask(threadpoll_t *poll,void*(*task)(void *arg),void *arg);

void  threadpoll_destroy(threadpoll_t *poll);

/* get number of live threads */
int threadpoll_get_liveNumber(threadpoll_t *poll);



#endif
