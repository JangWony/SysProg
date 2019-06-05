#include<stdio.h>
#include "csapp.h"

#define MAX_OBJECT_SIZE 102400

typedef struct {
    char cache_obj[MAX_OBJECT_SIZE];
    char cache_url[MAXLINE];
    int LRU;
    int isEmpty;

    int readCnt;            /*count of readers*/
    sem_t wmutex;           /*protects accesses to cache*/
    sem_t rdcntmutex;       /*protects accesses to readcnt*/

    int writeCnt;
    sem_t wtcntMutex;
    sem_t queue;

}cache_block;

typedef struct {
    cache_block cacheobjs[10];  /*ten cache blocks*/
    int cache_num;
}Cache;

void cache_init(Cache *cache);
int cache_find(Cache *cache,char *url);
int cache_eviction(Cache *cache);
void cache_LRU(Cache *cache,int index);
void cache_uri(Cache *cache,char *uri,char *buf);
void readerPre(Cache *cache,int i);
void readerAfter(Cache *cache,int i);