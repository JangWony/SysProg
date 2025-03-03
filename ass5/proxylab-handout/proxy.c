#include <stdio.h>
#include "csapp.h"
#include "cache.h"

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
void parse_uri(char *uri, char *hostname, char *path, int *port);
void handle(int conn_fd);

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);


Cache cache;

int main(int argc, char **argv){

    int listen_fd, conn_fd;
    pthread_t tid;
    socklen_t clientlen;
    char hostname[MAXLINE],port[MAXLINE];
    struct sockaddr_storage sockaddr;

    cache_init(&cache);

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
    return 0;
}

void *thread(void *vargp) {
    int conn_fd = (int)vargp;
    Pthread_detach(pthread_self());
    handle(conn_fd);
    Close(conn_fd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port){

    *port = 80;
    char* pos = strstr(uri,"//");

    pos = pos!=NULL? pos+2:uri;

    char*pos2 = strstr(pos,":");
    if(pos2!=NULL)
    {
        *pos2 = '\0';
        sscanf(pos,"%s",hostname);
        sscanf(pos2+1,"%d%s",port,path);
    }
    else
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}

void handle(int conn_fd){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];
    char endserver_http_header [MAXLINE];
    int portNumber;

    int end_serverfd;
    int client_fd =0;
    rio_t rio,rio_host;
    Rio_readinitb(&rio, conn_fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    
    sscanf(buf, "%s %s %s", method, uri, version);

    char url_store[100];
    strcpy(url_store, uri); 
    if(strcasecmp(method,"GET")){
        printf("Proxy does not implement the method");
        return;
    }
    int cache_index;
    if((cache_index=cache_find(&cache, url_store))!=-1){
        readerPre(&cache,cache_index);
        Rio_writen(conn_fd,cache.cacheobjs[cache_index].cache_obj,strlen(cache.cacheobjs[cache_index].cache_obj));
        readerAfter(&cache,cache_index);
        cache_LRU(&cache,cache_index);
        return;
    }

    parse_uri(uri, hostname, pathname, &portNumber);

    build_http_header(endserver_http_header,hostname,pathname,portNumber,&rio);

    end_serverfd = connect_endServer(hostname,portNumber,endserver_http_header);
    if(end_serverfd<0){
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&rio_host, end_serverfd);
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

    if(sizebuf < MAX_OBJECT_SIZE){
        cache_uri(&cache,url_store,cachebuf);
    }
    return;
    
}

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
   
    sprintf(request_hdr,requestlint_hdr_format,path);

    while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,endof_hdr)==0) break;

        if(!strncasecmp(buf,host_key,strlen(host_key)))
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

inline int connect_endServer(char *hostname,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(hostname,portStr);
}

