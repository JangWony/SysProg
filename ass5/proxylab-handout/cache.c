#include <stdio.h>

#include "cache.h"

// Cache function
void cache_init(Cache *cache){
    cache->cache_num = 0;
    int i;
    for(i=0;i<10;i++){
        cache->cacheobjs[i].LRU = 0;
        cache->cacheobjs[i].isEmpty = 1;
        Sem_init(&cache->cacheobjs[i].wmutex,0,1);
        Sem_init(&cache->cacheobjs[i].rdcntmutex,0,1);
        cache->cacheobjs[i].readCnt =         0;

        cache->cacheobjs[i].writeCnt = 0;
        Sem_init(&cache->cacheobjs[i].wtcntMutex,0,1);
        Sem_init(&cache->cacheobjs[i].queue,0,1);
    }
}

void readerPre(Cache *cache,int i){
    P(&cache->cacheobjs[i].queue);
    P(&cache->cacheobjs[i].rdcntmutex);
    cache->cacheobjs[i].readCnt++;
    if(cache->cacheobjs[i].readCnt==1) P(&cache->cacheobjs[i].wmutex);
    V(&cache->cacheobjs[i].rdcntmutex);
    V(&cache->cacheobjs[i].queue);
}

void readerAfter(Cache *cache,int i){
    P(&cache->cacheobjs[i].rdcntmutex);
    cache->cacheobjs[i].readCnt--;
    if(cache->cacheobjs[i].readCnt==0) V(&cache->cacheobjs[i].wmutex);
    V(&cache->cacheobjs[i].rdcntmutex);

}

void writePre(Cache *cache,int i){
    P(&cache->cacheobjs[i].wtcntMutex);
    cache->cacheobjs[i].writeCnt++;
    if(cache->cacheobjs[i].writeCnt==1) P(&cache->cacheobjs[i].queue);
    V(&cache->cacheobjs[i].wtcntMutex);
    P(&cache->cacheobjs[i].wmutex);
}

void writeAfter(Cache *cache,int i){
    V(&cache->cacheobjs[i].wmutex);
    P(&cache->cacheobjs[i].wtcntMutex);
    cache->cacheobjs[i].writeCnt--;
    if(cache->cacheobjs[i].writeCnt==0) V(&cache->cacheobjs[i].queue);
    V(&cache->cacheobjs[i].wtcntMutex);
}

/*find url is in the cache or not */
int cache_find(Cache *cache,char *url){
    int i;
    for(i=0;i<10;i++){
        readerPre(cache,i);
        if((cache->cacheobjs[i].isEmpty==0) && (strcmp(url,cache->cacheobjs[i].cache_url)==0)) break;
        readerAfter(cache,i);
    }
    if(i>=10) return -1; /*can not find url in the cache*/
    return i;
}

/*find the empty cacheObj or which cacheObj should be evictioned*/
int cache_eviction(Cache *cache){
    int min = 9999;
    int minindex = 0;
    int i;
    for(i=0; i<10; i++)
    {
        readerPre(cache,i);
        if(cache->cacheobjs[i].isEmpty == 1){/*choose if cache block empty */
            minindex = i;
            readerAfter(cache,i);
            break;
        }
        if(cache->cacheobjs[i].LRU< min){    /*if not empty choose the min LRU*/
            minindex = i;
            readerAfter(cache,i);
            continue;
        }
        readerAfter(cache,i);
    }

    return minindex;
}
/*update the LRU number except the new cache one*/
void cache_LRU(Cache *cache,int index){

    writePre(cache,index);
    cache->cacheobjs[index].LRU = 9999;
    writeAfter(cache, index);

    int i;
    for(i=0; i<index; i++)    {
        writePre(cache,i);
        if(cache->cacheobjs[i].isEmpty==0 && i!=index){
            cache->cacheobjs[i].LRU--;
        }
        writeAfter(cache, i);
    }
    i++;
    for(; i<10; i++)    {
        writePre(cache,i);
        if(cache->cacheobjs[i].isEmpty==0 && i!=index){
            cache->cacheobjs[i].LRU--;
        }
        writeAfter(cache, i);
    }
}
/*cache the uri and content in cache*/
void cache_uri(Cache *cache, char *uri,char *buf){


    int i = cache_eviction(cache);

    writePre(cache,i);/*writer P*/

    strcpy(cache->cacheobjs[i].cache_obj,buf);
    strcpy(cache->cacheobjs[i].cache_url,uri);
    cache->cacheobjs[i].isEmpty = 0;

    writeAfter(cache, i);/*writer V*/

    cache_LRU(cache,i);
}