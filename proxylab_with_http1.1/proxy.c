/*
 * proxy.c - ICS Web proxy
 * student number:517021910679
 * name:Haoyu wang
 */


#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Function prototypes
 */

struct the_information
{
    int connfd;
    struct sockaddr_in clientaddr;
};
char *the_connfd;
char logstring[MAXLINE],hostname[MAXLINE], portname[MAXLINE];
int parse_uri(char *uri, char *target_addr, char *pathname, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

void doit(int connfd, struct sockaddr_in *sockaddr);

void *thread(void*);

sem_t mutex;


/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    
    
   /* check arguments */
    if (argc != 2){
        exit(1);
    }

    //various declaration
    
    struct sockaddr_in clientaddr;
    
    Signal(SIGPIPE, SIG_IGN);
    Sem_init(&mutex, 0, 1);
    
    char* the_port=argv[1];
    
    int listenfd = open_listenfd(the_port);
 
    
    
    pthread_t tid;
    
    while(1){
       socklen_t clientlen = sizeof(clientaddr);
        struct the_information *mes = (struct the_information*)malloc(sizeof(struct the_information));
        
        the_connfd = Malloc(sizeof(int) + sizeof(uint32_t));
        *((int*)the_connfd) = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        int flags = NI_NUMERICHOST;
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, portname, MAXLINE, flags);
        inet_pton(AF_INET, hostname, the_connfd+sizeof(int));
        
        Pthread_create(&tid, NULL, thread,the_connfd);
    }
    close(listenfd);
    exit(0);
}


/*
 * thread
 */
void *thread(void *vargp)
{
    
    struct the_information *mes = (struct the_information *)vargp;
    pthread_detach(pthread_self());
    struct in_addr addr;
    addr.s_addr = *((uint32_t*)(vargp+sizeof(int)));
    mes->clientaddr.sin_family = AF_INET;
    mes->clientaddr.sin_addr=addr;
    doit(mes->connfd,&mes->clientaddr);
    
    close(mes->connfd);
    free(mes);
    return NULL;
}

/*
 * doit - handle the client HTTP transaction
 */
void doit(int connfd, struct sockaddr_in *sockaddr)
{
    //variable
    
    char client_buf[MAXLINE], method[16], uri[MAXLINE], version[16];
    
    char target[256], pathname[MAXLINE], port[MAXLINE];
    rio_t rio_client;
    
    
    

  
    //read request line
    rio_readinitb(&rio_client, connfd);
    int read_the_response = rio_readlineb(&rio_client, client_buf, MAXLINE);
    
    
    sscanf(client_buf, "%s %s %s", method, uri, version); 
   
    if (strcmp(version,"HTTP/1.1"))
    {
        return;
    } 
    if (parse_uri(uri, target, pathname, port) != 0){
        return;
    }
    if (0>=read_the_response) {
        return;
    }
     //read headers from client and forward to server
    int clientfd;
    char request[MAXLINE];
    int request_body_length = 0;
    sprintf(request, "%s /%s %s\r\n", method, pathname, version);
    
    while ((read_the_response = rio_readlineb(&rio_client, client_buf, MAXLINE)))
    {
        
        
        if (!strncasecmp(client_buf, "Content-Length", 14)){
            request_body_length = atoi(client_buf + 15);
        }
        sprintf(request, "%s%s", request, client_buf);
        if (!strcmp(client_buf, "\r\n")){
            break;
        }
       
        if (0>=read_the_response){
            close(clientfd);
            return;

        }
    }
    if (0>=read_the_response ){
        return;
    }
    
   
    //establish the connection
    rio_t rio_server;
    int write_the_response;;
    clientfd = open_clientfd(target, port);
    
    
    rio_readinitb(&rio_server, clientfd);
    
    write_the_response = rio_writen(clientfd, request, strlen(request));
    if (strlen(request) !=write_the_response )
    {
        close(clientfd);
        return;
    }
    if (0>=clientfd )
    {
        close(clientfd);
        return;
    }
    
    //read body from client and forward to server
    int temp_figure;
    if (request_body_length!=0 ){
        temp_figure = request_body_length;
        while (0<temp_figure ){
            read_the_response = rio_readnb(&rio_client, client_buf, 1);
            if ((read_the_response <= 0) || (temp_figure <=0)||rio_writen(clientfd, client_buf, 1)<=0){
                close(clientfd);
                return;
            }
            temp_figure -= 1;
        }
    }
    
    //receive response from server and forward to the client
    char server_buf[MAXLINE];
    int bodysize_of_response = 0, headsize_of_response = 0;
    while ((temp_figure = rio_readlineb(&rio_server, server_buf, MAXLINE))){
        if (!strncasecmp(server_buf, "Content-Length", 14)){
            bodysize_of_response = atoi(server_buf + 15);
        }
        headsize_of_response += temp_figure;
        write_the_response = rio_writen(connfd, server_buf, strlen(server_buf));
        
        if (!strcmp(server_buf, "\r\n")){
            break;
        }
         if (0>=temp_figure ){
            close(clientfd);
            return;
        }
    }
    
    if (temp_figure == 0){
        close(clientfd);
        return;
    }
    
    /* forward the response body to client */
    temp_figure = bodysize_of_response;
    while (0<temp_figure )
    {
        read_the_response = rio_readnb(&rio_server, server_buf, 1);
        if ((0>=read_the_response ) && (1<temp_figure )){
            close(clientfd);
            return;
        }
        
        write_the_response = rio_writen(connfd, server_buf, 1);
        if (write_the_response != 1){
            close(clientfd);
            return;
        }
        
        temp_figure --;
    }
    close(clientfd);
    
    
    /* Print log */
   
    P(&mutex);
    int sum_size=bodysize_of_response + headsize_of_response;
    format_log_entry(logstring, sockaddr, uri, sum_size);
    
    
    printf("%s\n", logstring);
    V(&mutex);
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, pathname name, and port.  The memory for hostname and
 * pathnamename must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathnamename, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathnamebegin;
    int len;
    
    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
    
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }
    
    /* Extract the pathname */
    pathnamebegin = strchr(hostbegin, '/');
    if (pathnamebegin == NULL) {
        pathnamename[0] = '\0';
    }
    else {
        pathnamebegin++;
        strcpy(pathnamename, pathnamebegin);
    }
    
    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;
    
    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
    
    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;
    
    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}





