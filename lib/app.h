
#ifndef LIBPAS_APP_H
#define LIBPAS_APP_H

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sched.h>

#include <signal.h>

#include <pthread.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "main.h"
#include "acp/main.h"
#include "acp/app.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define LIST_GET_BY_ID \
     int i;\
    for (i = 0; i < list->length; i++) {\
        if (list->item[i].id == id) {\
            return &(list->item[i]);\
        }\
    }\
    return NULL;
#define LIST_GET_BY_IDSTR \
     int i;\
    for (i = 0; i < list->length; i++) {\
        if (strcmp(list->item[i].id, id)==0) {\
            return &(list->item[i]);\
        }\
    }\
    return NULL;

#define LLIST_GET_BY_ID(T) \
    T *curr = list->top;\
    while(curr!=NULL){\
        if(curr->id==id){\
            return curr;\
        }\
        curr=curr->next;\
    }\
    return NULL;

#define LIST_GET_BY(V) \
     int i;\
    for (i = 0; i < list->length; i++) {\
        if (list->item[i].V == id) {\
            return &(list->item[i]);\
        }\
    }\
    return NULL;

#define FORL for (i = 0; i < list->length; i++) 
#define LIi list->item[i]

#define FREE_LIST(list) free((list)->item); (list)->item=NULL; (list)->length=0;

#define FUN_LIST_GET_BY(V,T) T *get##T##By_##V (int id, const T##List *list) {  LIST_GET_BY(V) }

#define FUN_LIST_GET_BY_ID(T) T *get ## T ## ById(int id, const T ## List *list) {  LIST_GET_BY_ID }
#define FUN_LIST_GET_BY_IDSTR(T) T *get ## T ## ById(char *id, const T ## List *list) {  LIST_GET_BY_IDSTR }
#define FUN_LLIST_GET_BY_ID(T) T *get ## T ## ById(int id, const T ## List *list) {  LLIST_GET_BY_ID(T) }

#define DEF_LIST(T) typedef struct {T *item; size_t length;} T##List;
#define DEF_LLIST(T) typedef struct {T *top; T *last; size_t length;} T##List;

#define FUN_LOCK(T) int lock ## T (T *item) {if (item == NULL) {return 0;} if (pthread_mutex_lock(&(item->mutex.self)) != 0) {return 0;}return 1;}
#define FUN_TRYLOCK(T) int tryLock ## T (T  *item) {if (item == NULL) {return 0;} if (pthread_mutex_trylock(&(item->mutex.self)) != 0) {return 0;}return 1;}
#define FUN_UNLOCK(T)int unlock ## T (T *item) {if (item == NULL) {return 0;} if (pthread_mutex_unlock(&(item->mutex.self)) != 0) {return 0;}return 1;}

enum {
    APP_INIT = 90,
    APP_INIT_DATA,
    APP_RUN,
    APP_STOP,
    APP_RESET,
    APP_EXIT
} State;


typedef struct {
    pthread_mutex_t self;
    pthread_mutexattr_t attr;
    int created;
    int attr_initialized;
} Mutex;

extern int readConf(const char *path, char conninfo[LINE_SIZE], char app_class[NAME_SIZE]);

extern void conSig(void (*fn)());

extern void setPriorityMax(int policy);

extern int readHostName(char *hostname) ;

extern int initPid(int *pid_file, int *pid, const char *pid_path) ;

extern void freePid(int *pid_file, int *pid, const char *pid_path) ;

extern int initMutex(Mutex *m) ;

extern void freeMutex(Mutex *m) ;

extern void waitThreadCmd(char *thread_cmd,char *thread_qfr, char *cmd);

#endif 

