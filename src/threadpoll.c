#include "../inc/threadpoll.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct task_s {
  void *(*func)(void *agr);
  void *arg;
} task_t;

typedef struct task_que_s {
  task_t *que;
  uint queSize;
  uint taskCount;
  uint queHead;
  uint quetail;
  pthread_mutex_t mutex;
  pthread_cond_t queFull;
  pthread_cond_t queEmpty;
} tasl_que_t;

typedef struct work_threads_s {
  uint size;
  uint busyNumber;
  uint liveNumber;
  pthread_t *threads;
  pthread_mutex_t mutex;
} work_threads_t;

typedef struct threadpoll_s {
  tasl_que_t taskQue;
  work_threads_t workThreads;
  char shutDown;
  /* manager thread id */
  pthread_t managerId;
} threadpoll_t;

int threadpoll_get_liveNumber(threadpoll_t *poll){
  pthread_mutex_lock(&poll->workThreads.mutex);
  int liveNumber = poll->workThreads.liveNumber;
  pthread_mutex_unlock(&poll->workThreads.mutex);
  return liveNumber;
}

void threadExit(threadpoll_t *poll,void *arg){
  pthread_t tid = pthread_self();
  for(int i=0;i<poll->workThreads.size;i++){
    if (poll->workThreads.threads[i] == tid) {
      poll->workThreads.threads[i]= 0;
      break;
    }
  }
  pthread_exit(arg);
}

void *manager(void *arg) {
  threadpoll_t *poll = (threadpoll_t *)arg;

  return NULL;
}

void *work(void *arg) {
  threadpoll_t *poll = (threadpoll_t *)arg;
  while (1) {
    pthread_mutex_lock(&poll->taskQue.mutex);
    /* If the queue is empty then the thread waits */
    while (poll->taskQue.queHead == poll->taskQue.quetail) {
      pthread_cond_wait(&poll->taskQue.queEmpty, &poll->taskQue.mutex);
      if (poll->shutDown) {
        pthread_mutex_unlock(&poll->taskQue.mutex);
        pthread_mutex_lock(&poll->workThreads.mutex);
        poll->workThreads.liveNumber--;
        pthread_mutex_unlock(&poll->workThreads.mutex);
        threadExit(poll,NULL);
      }
    }
    /* get task from taskQue */
    task_t task = poll->taskQue.que[poll->taskQue.queHead];
    poll->taskQue.queHead++;
    poll->taskQue.queHead %= poll->taskQue.queSize;
    poll->taskQue.taskCount--;
    pthread_mutex_unlock(&poll->taskQue.mutex);

    /* notify  producer*/
    pthread_cond_signal(&poll->taskQue.queFull);

    /* busy thread add 1 */
    pthread_mutex_lock(&poll->workThreads.mutex);
    poll->workThreads.busyNumber++;
    pthread_mutex_unlock(&poll->workThreads.mutex);

    /* run task */
    task.func(task.arg);
    free(task.arg);

    /* busy thread delete  1 */
    pthread_mutex_lock(&poll->workThreads.mutex);
    poll->workThreads.busyNumber--;
    pthread_mutex_unlock(&poll->workThreads.mutex);
  }
  return NULL;
}

threadpoll_t *threadpoll_get(uint queSize, int max, int min) {
  static threadpoll_t poll;
  if (poll.workThreads.threads && poll.taskQue.que) {
    return &poll;
  }

  while (1) {

    /* init task queue */
    poll.taskQue.que = (task_t *)malloc(sizeof(task_t) * queSize);
    if (poll.taskQue.que == NULL) {
      printf("taskQue que malloc fail\n");
      break;
    }
    poll.taskQue.queSize = queSize;
    poll.taskQue.taskCount = 0;
    poll.taskQue.queHead = 0;
    poll.taskQue.quetail = 0;
    pthread_mutex_init(&poll.taskQue.mutex, NULL);
    pthread_cond_init(&poll.taskQue.queFull, NULL);
    pthread_cond_init(&poll.taskQue.queEmpty, NULL);

    /* init work threads */
    poll.workThreads.threads = (pthread_t *)malloc(sizeof(pthread_t) * max);
    if (poll.workThreads.threads == NULL) {
      printf("workThreads malloc fail\n");
      break;
    }
    memset(poll.workThreads.threads, 0, sizeof(pthread_t) * max);
    poll.workThreads.size = max;
    poll.workThreads.liveNumber = min;
    poll.workThreads.busyNumber = 0;
    pthread_mutex_init(&poll.workThreads.mutex, NULL);

    poll.shutDown = 0;
    /* run work thread */
    for (int i = 0; i < poll.workThreads.liveNumber; i++){
      pthread_create(&poll.workThreads.threads[i], NULL, work, &poll);
      pthread_detach(poll.workThreads.threads[i]);
    }

    /* run manager thread */
    /* pthread_create(&poll.managerId, NULL, manager, &poll); */
    return &poll;
  }

  if (poll.taskQue.que) {
    free(poll.taskQue.que);
    poll.taskQue.que = NULL;
  }

  if (poll.workThreads.threads) {
    free(poll.workThreads.threads);
    poll.workThreads.threads = NULL;
  }
  return NULL;
}

int threadpoll_addTask(threadpoll_t *poll, void *(*task)(void *arg), void *arg) {
  pthread_mutex_lock(&poll->taskQue.mutex);
  while ((poll->taskQue.quetail + 1) % poll->taskQue.queSize ==
         poll->taskQue.queHead) {
    /* if queue is full than thread wait */
    pthread_cond_wait(&poll->taskQue.queFull, &poll->taskQue.mutex);
    if (poll->shutDown) {
      pthread_mutex_unlock(&poll->taskQue.mutex);
      return 0;
    }
  }

  /* add task to taskQue */
  poll->taskQue.que[poll->taskQue.quetail].func = task;
  poll->taskQue.que[poll->taskQue.quetail].arg = arg;
  poll->taskQue.quetail++;
  poll->taskQue.quetail %= poll->taskQue.queSize;
  poll->taskQue.taskCount++;
  pthread_mutex_unlock(&poll->taskQue.mutex);
  pthread_cond_signal(&poll->taskQue.queEmpty);
  return 1;
}

void threadpoll_destroy(threadpoll_t *poll) {
  poll->shutDown = 1;
  while (threadpoll_get_liveNumber(poll)) {
    pthread_cond_signal(&poll->taskQue.queEmpty);
  }
  
  /* release mutex and cond */
  pthread_mutex_destroy(&poll->taskQue.mutex);
  pthread_mutex_destroy(&poll->workThreads.mutex);

  pthread_cond_destroy(&poll->taskQue.queEmpty);
  pthread_cond_destroy(&poll->taskQue.queFull);

  /* free memory */
  free(poll->taskQue.que);
  poll->taskQue.que = NULL;
  free(poll->workThreads.threads);
  poll->workThreads.threads = NULL;
}
