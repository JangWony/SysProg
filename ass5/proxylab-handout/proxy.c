#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void *thread(void *vargp);
int parse_uri(char *uri, char *hostname, char *query, int *port);
void handle(int conn_fd);

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void Rio_writeb_w(int fd, void *usrbuf, size_t n);

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);

void cache_init();
int cache_find(char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_uri(char *uri,char *buf);
void readerPre(int i);
void readerAfter(int i);

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

Cache cache;

int main(int argc, char **argv){

    int listen_fd, conn_fd;
    pthread_t tid;
    socklen_t clientlen;
    char hostname[MAXLINE],port[MAXLINE];
    struct addrinfo sockaddr;

    cache_init();

    if(argc != 2){
        fprintf(stderr, "Usage: %s <port> \n", argv[0]);
        exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);

    listen_fd = Open_listenfd(argv[1]);

    while(1){
        clientlen = sizeof(sockaddr);
        conn_fd = Accept(listen_fd, (SA *)&sockaddr, &clientlen);
        Getnameinfo((SA*)&sockaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accepted connection from (%s %s).\n",hostname,port);
        Pthread_create(&tid, NULL, thread, (void *)conn_fd);
    }

    Close(listen_fd);
    exit(0);
}

void *thread(void *vargp) {
    int conn_fd = (int)vargp;
    Pthread_detach(pthread_self());
    handle(conn_fd);
    Close(conn_fd);
}

int parse_uri(char *uri, char *hostname, char *query, int *port){

    char *hoststart, *hostend, *pathstart;
    int length;

    if(strncasecmp(uri, "http://", 7)!=0){
        hostname[0] = '\0';
        return -1;
    }

    hoststart = uri + 7;
    hostend = strpbrk(hoststart, " :/\rn\n\0");
    length = hostend - hoststart;
    strncpy(hostname, hoststart, length);
    hostname[length] = '\0';

    *port= 80;
    if(*hostend == ':'){
        *port = atoi(hostend + 1);
    }
    
    pathstart = strchr(hoststart, '/');
    if(pathstart == NULL){
        query[0] = '\0';
    }
    else{
        pathstart++;
        strncpy(query, pathstart, MAXLINE);
    }
    return 0;
}

void handle(int conn_fd){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];/*, log[MAXLINE]*/
    char endserver_http_header [MAXLINE];
    int n, portNumber;

    int end_serverfd;
    int client_fd =0;
    rio_t rio,rio_host;
    Rio_readinitb(&rio, conn_fd);
    Rio_readlineb(&rio, buf, MAXLINE))
    
    sscanf(buf, "%s %s %s", method, uri, version);

    char url_store[100];
    strcpy(url_store, uri); // store the original url
    if(strcasecmp(method,"GET")){
        printf("Proxy does not implement the method");
        return;
    }
    int cache_index;
    if((cache_index=cache_find(url_store))!=-1){/*in cache then return the cache content*/
        readerPre(cache_index);
        Rio_writen(conn_fd,cache.cacheobjs[cache_index].cache_obj,strlen(cache.cacheobjs[cache_index].cache_obj));
        readerAfter(cache_index);
        cache_LRU(cache_index);
        return;
    }

    parse_uri(uri, hostname, pathname, &portNumber);
    //    printf("uri: %s\nhostname: %s\npathname: %s\n port: %d\n", uri, hostname, pathname, portNumber);

    build_http_header(endserver_http_header,hostname,pathname,portNumber,&rio);

    /*connect to the end server*/
    end_serverfd = connect_endServer(hostname,portNumber,endserver_http_header);
    if(end_serverfd<0){
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&rio_host, client_fd);
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    char cachebuf[MAX_OBJECT_SIZE];
    int sizebuf = 0;
    size_t n;
    while((n=Rio_readlineb(&rio_host,buf,MAXLINE))!=0)
    {
        sizebuf+=n;
        if(sizebuf < MAX_OBJECT_SIZE) strcat(cachebuf,buf);
        Rio_writen(conn_fd,buf,n);
    }
    Close(end_serverfd);

    /*store it*/
    if(sizebuf < MAX_OBJECT_SIZE){
        cache_uri(url_store,cachebuf);
    }
    return;
    
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen){
    ssize_t ret;
    if((ret = rio_readlineb(rp, usrbuf, maxlen)) < 0){
        printf("Warning: Rio_readlineb error.\n");
        return 0;
    }
    return ret; 
}

void Rio_writeb_w(int fd, void *usrbuf, size_t n){
    if(rio_writen(fd, usrbuf, n)!= n){
        printf("Warning: Rio_writen failed.\n");
        return;
    }
}

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
    /*request line*/
    sprintf(request_hdr,requestlint_hdr_format,path);
    /*get other request header for client rio and change it */
    while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,endof_hdr)==0) break;/*EOF*/

        if(!strncasecmp(buf,host_key,strlen(host_key)))/*Host:*/
        {
            strcpy(host_hdr,buf);
            continue;
        }

        if(!strncasecmp(buf,connection_key,strlen(connection_key))
                &&!strncasecmp(buf,proxy_connection_key,strlen(proxy_connection_key))
                &&!strncasecmp(buf,user_agent_key,strlen(user_agent_key)))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,host_hdr_format,hostname);
    }
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return ;
}
/*Connect to the end server*/
inline int connect_endServer(char *hostname,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(hostname,portStr);
}

// Cache function
void cache_init(){
    cache.cache_num = 0;
    int i;
    for(i=0;i<10;i++){
        cache.cacheobjs[i].LRU = 0;
        cache.cacheobjs[i].isEmpty = 1;
        Sem_init(&cache.cacheobjs[i].wmutex,0,1);
        Sem_init(&cache.cacheobjs[i].rdcntmutex,0,1);
        cache.cacheobjs[i].readCnt = 0;

        cache.cacheobjs[i].writeCnt = 0;
        Sem_init(&cache.cacheobjs[i].wtcntMutex,0,1);
        Sem_init(&cache.cacheobjs[i].queue,0,1);
    }
}

void readerPre(int i){
    P(&cache.cacheobjs[i].queue);
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt++;
    if(cache.cacheobjs[i].readCnt==1) P(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);
    V(&cache.cacheobjs[i].queue);
}

void readerAfter(int i){
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt--;
    if(cache.cacheobjs[i].readCnt==0) V(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);

}

void writePre(int i){
    P(&cache.cacheobjs[i].wtcntMutex);
    cache.cacheobjs[i].writeCnt++;
    if(cache.cacheobjs[i].writeCnt==1) P(&cache.cacheobjs[i].queue);
    V(&cache.cacheobjs[i].wtcntMutex);
    P(&cache.cacheobjs[i].wmutex);
}

void writeAfter(int i){
    V(&cache.cacheobjs[i].wmutex);
    P(&cache.cacheobjs[i].wtcntMutex);
    cache.cacheobjs[i].writeCnt--;
    if(cache.cacheobjs[i].writeCnt==0) V(&cache.cacheobjs[i].queue);
    V(&cache.cacheobjs[i].wtcntMutex);
}

/*find url is in the cache or not */
int cache_find(char *url){
    int i;
    for(i=0;i<10;i++){
        readerPre(i);
        if((cache.cacheobjs[i].isEmpty==0) && (strcmp(url,cache.cacheobjs[i].cache_url)==0)) break;
        readerAfter(i);
    }
    if(i>=10) return -1; /*can not find url in the cache*/
    return i;
}

/*find the empty cacheObj or which cacheObj should be evictioned*/
int cache_eviction(){
    int min = 9999;
    int minindex = 0;
    int i;
    for(i=0; i<10; i++)
    {
        readerPre(i);
        if(cache.cacheobjs[i].isEmpty == 1){/*choose if cache block empty */
            minindex = i;
            readerAfter(i);
            break;
        }
        if(cache.cacheobjs[i].LRU< min){    /*if not empty choose the min LRU*/
            minindex = i;
            readerAfter(i);
            continue;
        }
        readerAfter(i);
    }

    return minindex;
}
/*update the LRU number except the new cache one*/
void cache_LRU(int index){

    writePre(index);
    cache.cacheobjs[index].LRU = 9999;
    writeAfter(index);

    int i;
    for(i=0; i<index; i++)    {
        writePre(i);
        if(cache.cacheobjs[i].isEmpty==0 && i!=index){
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
    i++;
    for(i; i<10; i++)    {
        writePre(i);
        if(cache.cacheobjs[i].isEmpty==0 && i!=index){
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
}
/*cache the uri and content in cache*/
void cache_uri(char *uri,char *buf){


    int i = cache_eviction();

    writePre(i);/*writer P*/

    strcpy(cache.cacheobjs[i].cache_obj,buf);
    strcpy(cache.cacheobjs[i].cache_url,uri);
    cache.cacheobjs[i].isEmpty = 0;

    writeAfter(i);/*writer V*/

    cache_LRU(i);
}