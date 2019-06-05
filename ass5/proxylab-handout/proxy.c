#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

int parse_uri(char *uri, char *hostname, char *query, int *port);
void handle(int conn_fd, int port, struct addrinfo *sockaddr);

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void Rio_writeb_w(int fd, void *usrbuf, size_t n);

void clear_log(char *logstr, struct addrinfo *sockaddr, char *uri, int size);

int main(int argc, char **argv){

    int listen_fd, conn_fd, port;
    socklen_t clientlen;
    struct addrinfo sockaddr;

    if(argc != 2){
        fprintf(stderr, "Usage: %s <port> \n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);

    listen_fd = Open_listenfd(port);

    while(1){
        clientlen = sizeof(sockaddr);
        conn_fd = Accept(listen_fd, (SA *)&sockaddr, &clientlen);
        handle(conn_fd, port, &sockaddr);
    }

    Close(conn_fd);
    Close(listen_fd);
    exit(0);
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

void handle(int conn_fd, int port, struct addrinfo *sockaddr){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];/*, log[MAXLINE]*/
    struct addrinfo *addr = sockaddr;
    int n, size, portNumber;

    int client_fd =0;
    rio_t rio,rio_host;
    Rio_readinitb(&rio, conn_fd);
    size = 0;

    if((n = Rio_readlineb_w(&rio, buf, MAXLINE))!=0){
        sscanf(buf, "%s %s %s", method, uri, version);
        parse_uri(uri, hostname, pathname, &portNumber);
        printf("uri: %s\nhostname: %s\npathname: %s\n port: %d\n", uri, hostname, pathname, portNumber);

        if((client_fd = Open_clientfd(hostname, portNumber)) < 0){
            return;
        }

        Rio_readinitb(&rio_host, client_fd);
        Rio_writeb_w(client_fd, buf, strlen(buf));

        while(strncmp((char*) buf, "\r\n",2)){
            Rio_readlineb(&rio, buf, MAXLINE);
            if(!strncmp((char*)buf, "\r\n",2)){
                char *str = "Connection: close\r\n";
                Rio_writeb_w(client_fd, str, strlen(str));
            }

            if(strncmp((char*)buf, "Keep-Alive:", 11) && strncmp((char*)buf, "Proxy-Connection:", 17)){
                Rio_writeb_w(client_fd, buf, strlen((char*)buf));
            }   
        }

        while((n= Rio_readlineb_w(&rio_host, buf, MAXLINE)) != 0){
            Rio_writeb_w(conn_fd, buf, n);
            size+= n;
        }
        /*
        clear_log(log, addr, uri, size);
        fprintf
        */
       return;
    }
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

void clear_log(char *logstr, struct addrinfo *sockaddr, char *uri, int size){

}