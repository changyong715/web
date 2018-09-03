/*************************************************************************
	> File Name: httpd.c
	> Author: 
	> Mail: 
	> Created Time: Sun 12 Aug 2018 04:16:30 AM PDT
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#define MAX 1024
#define MAIN_PAGE "INdex.html"
#define PAGE_404 "wwwroot/404.html"

/*int startup(int port)
{
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0){
        perror("socket");
        return 2;
    }
    
    int opt=1;//防止服务器主动断开连接，导致服务器处于TIME_WAIT状态
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in local;
    local.sin_family=AF_INET;
    local.sin_addr.s_addr=htonl(INADDR_ANY);
    local.sin_port=htons(port);

    if(bind(sock,(struct sockaddr*)&local,sizeof(local)) < 0){
        perror("bind");
        return 3;
    }

    if(listen(sock,5) < 0){
        perror("listen");
        return 4;
    } 
    
    return sock;
}*/

int GetLine(int sock,char line[],int len)//按行获取
{
    char c='a';
    int i=0;
    while(c != '\n' && (i < len -1 ))
    {
        ssize_t ret=recv(sock,&c,1,0);
        if(ret > 0)
        { //目的，将\r \r\n \n 转为 \n
            if(c == '\r')
            {
                recv(sock,&c,1,MSG_PEEK);//窥探，读取下一个字符
                if(c == '\n')
                {
                    recv(sock,&c,1,0);
                }
                else
                    c = '\n'; 
            }// if(ret>0 && c == '\r')
            line[i++]=c;
        }
        else 
            break;
    }//while

    line[i] = '\0';
    return i;
}

void ClearHeader(int sock)
{
    printf("begin ClearHeader\n");
    char line[MAX];
    do{
        GetLine(sock,line,sizeof(line));
    }while(strcmp(line,"\n"));
    printf("after ClearHeader\n");
}

int echo_resource(int sock,char* path,int size)
{
    printf("echo_resource\n");
    char line[MAX];
    ClearHeader(sock);
    int fd=open(path,O_RDONLY);
    if(fd < 0){
        return 500;
    }

    sprintf(line,"HTTP/1.0 200 OK\r\n");
    send(sock,line,strlen(line),0);
    char *p = path + strlen(path) -1;
    while(*p != '.')
        p--;
    if(strcmp(p,".css") == 0)
        sprintf(line,"Content-Type: text/css\r\n");
    else if(strcmp(p,".js") == 0)
        sprintf(line,"Content-Type: application/x-javascript\r\n");
    else
        sprintf(line,"Content-Type:text/html;application/x-csi;application/x-jpg\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"Content-Length: %d\r\n",size);
    send(sock,line,strlen(line),0);
    sprintf(line,"\r\n");
    send(sock,line,strlen(line),0);
    sendfile(sock,fd,NULL,size);
    close(fd);
}

int exe_cgi(int sock,char* method,char* path,char* query_string)
{
    char line[MAX];
    int content_length=-1;
    char method_env[MAX/16];
    char query_string_env[MAX];
    char content_length_env[MAX];
    if(strcasecmp(method,"GET") == 0)
    {
        ClearHeader(sock);
    }
    else
    {
        do{
            GetLine(sock,line,sizeof(line));
            //读取Content-Length,解决粘包问题
            if(strncmp(line,"Content-Length: ",16))
            {
                content_length=atoi(line+16);//读取长度
            }
        }while(strcmp(line,"\n")); 

        if(-1 == content_length)//没有Content-Length
        {
            return 400;
        }
    }
   
    //创建管道
    int input[2];//读
    int output[2];//写
    pipe(input);
    pipe(output);
    
    signal(SIGPIPE,SIG_IGN);
    pid_t id;
    id = fork();
    if(id < 0)
        return 500;


    else if(0 == id)//child
    {
        close(input[1]);//子进程读取父进程，所以关掉管道写端
        close(output[0]);//子进程给父进程写，关掉管道读端
        
        dup2(input[0],0);
        dup2(output[1],1);

        sprintf(method_env,"METHOD=%s",method);
        putenv(method_env);
        if(0 == strcasecmp(method,"GET"))
        {
            sprintf(query_string_env,"QUERY_STRING=%s",query_string);
            putenv(query_string_env);
        }
        else
        {
            sprintf(content_length_env,"CONTENT_LENGTH=%d",content_length);
            putenv(content_length_env);
        }
        execl(path,path,NULL);
        exit(1);
    }
    else//父进程任务：数据从网络读取到子进程，从子进程读取数据到网络
    {
        close(input[0]);
        close(output[1]);
        
        int i=0;
        char c;
        if(0 == strcasecmp(method,"POST"))
        {
            for(; i< content_length; ++i)
            {
                recv(sock,&c,1,0);
                write(input[1],&c,1);
            }
        }
            
      sprintf(line,"HTTP/1.0 200 OK\r\n");
      send(sock,line,strlen(line),0);
      sprintf(line,"Content-Type: text/html;application/x-csi;\r\n");
      send(sock,line,strlen(line),0);
      sprintf(line,"\r\n");
      send(sock,line,strlen(line),0);

      while(read(output[0],&c,1) > 0)
        {
            send(sock,&c,1,0);
        }

        waitpid(id,NULL,0);

        close(input[1]);
        close(output[0]);
    }
    return 200;
}

void echo_404(int sock)
{
    char line[MAX];
    struct stat st;
    stat(PAGE_404,&st);
    sprintf(line,"HTTP/1.0 404 Not Find\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"Content-Type: text/css\r\n");
    sprintf(line,"Content-Type: application/x-javascript\r\n");
    sprintf(line,"Content-Type:text/html;application/x-csi;application/x-jpg\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"Content-Length: %d\r\n",st.st_size);
    send(sock,line,strlen(line),0);
    sprintf(line,"\r\n");
    send(sock,line,strlen(line),0);


    int fd = open(PAGE_404,O_RDONLY);
    sendfile(sock,fd,NULL,st.st_size);
    close(fd);
}

void EchoError(int sock,int statuscode)
{
    switch(statuscode)
    {
        case 400:
            break;
        case 404:
            echo_404(sock);
            break;
        case 403:
            break;
        case 500:
            break;
        default:
            break;
    }//switch
}

void handlerRequest(int arg)
{
    int sock=arg;
    char line[MAX];
    char method[MAX/16];
    char url[MAX];
    char path[MAX];
    int i=0,j=0;
    int statuscode=200;
    int cgi=0;
    char* query_string=NULL;

    //HTTP第一行：method url http_version
    GetLine(sock,line,MAX);
    while(i < sizeof(method)-1 && j < sizeof(line) && line[j]!= ' ')//处理头部信息
    {
        method[i++]=line[j++]; 
    } //while
    method[i]='\0';
    //处理方法GET POST
    if(0 == strcasecmp(method,"GET"))
    {
        printf("是GET方法\n");
    }
    else if(0 == strcasecmp(method,"POST"))
    {
        printf("是POST方法\n");
        cgi = 1; 
    }
    else//清理报头(处理完请求再给响应)
    {
        printf("before ClearHeader\n");
        //ClearHeader(sock);
        printf("ClearHeader\n");
        statuscode=400;
        goto end;
    }

    //取url信息
    i=0;
    while(j < sizeof(line) && line[j] == ' ')
    {
        j++;
    }
    while(i < sizeof(url) && j < sizeof(line) && line[j] != ' ')
    {
        url[i++]=line[j++];
    }
    url[i]='\0';
    printf("method:%s url:%s\n",method,url);
    
    if(0 == strcasecmp(method,"GET"))  //如果是GET方法，参数在？后面
    {
        query_string = url;
        while(*query_string){
            if(*query_string == '?')
            {
                *query_string = '\0';
                query_string++;
                cgi=1;
                break;
            }
            query_string++;
        }
    }

    //取得了method url(资源和参数已分离)
    sprintf(path,"wwwroot%s",url);
    if(path[strlen(path)-1] == '/'){
        strcat(path,MAIN_PAGE);
    }
    printf("method:%s,url:%s,query_string:%s\n",method,path,query_string);
    
    struct stat st;
    if(stat(path,&st) < 0){//判断文件属性
        ClearHeader(sock);
        statuscode = 404;
        goto end;
    }
    else{
        if(S_ISDIR(st.st_mode)){//判断一个路径是否为目录
            strcat(path,"/");
            strcat(path,MAIN_PAGE);
        }
        else if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            cgi=1;
        else
          {}
    }//else
    if(cgi){
        statuscode=exe_cgi(sock,method,path,query_string);        
        printf("处理完CGI响应\n");
    }
    else{
        statuscode = echo_resource(sock,path,st.st_size);
    }

end:
    if(statuscode != 200)
    {
        EchoError(sock,statuscode);
    }
    close(sock);
}

void ProcessConnect(int listen_sock,int epoll_fd)
{
    int sock;
    struct sockaddr_in client;
    socklen_t len=sizeof(client);
    sock=accept(listen_sock,(struct sockaddr*)&client,&len);
    if(sock < 0){
        perror("accept");
        return;
    }
    struct epoll_event ev;
    ev.data.fd=sock;
    ev.events=EPOLLIN;
    int ret=epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock,&ev);
    if(ret < 0){
        perror("epoll_ctl");
        return;
    }


    printf("----------------------------\n");
    printf("| 添加新链接,EPOLL_CTL_ADD |\n");
    printf("|                          |\n");
    printf("----------------------------\n");
    return;
}

void ProcessRequest(int connect_fd,int epoll_fd)
{
    handlerRequest(connect_fd);   
    close(connect_fd);
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,connect_fd,NULL);
    printf("###########################\n");
    printf("#链接处理完,EPOLL_CTL_DEL #\n");
    printf("#                         #\n");
    printf("###########################\n");
    return;
}

int main(int argc,char* argv[])
{
    daemon(1,1);//守护进程

    if(argc!=2){
        printf("Usage: %s port\n",argv[0]);
        return 1;
    }

    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0){
        perror("socket");
        return 2;
    }
    
    int opt=1;//防止服务器主动断开连接，导致服务器处于TIME_WAIT状态
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt)); 
    struct sockaddr_in local;
    local.sin_family=AF_INET;
    local.sin_addr.s_addr=htonl(INADDR_ANY);
    local.sin_port=htons(atoi(argv[1]));

    if(bind(sock,(struct sockaddr*)&local,sizeof(local)) < 0){
        perror("bind");
        return 3;
    }

    if(listen(sock,100) < 0){
        perror("listen");
        return 4;
    } 
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    int epoll_fd=epoll_create(FD_SETSIZE);//创建epoll模型
    if(epoll_fd < 0){
        perror("epoll_create");
        return 1;
    }

    struct epoll_event event;

    event.events=EPOLLIN;
    //event.data.fd=listen_sock;
    event.data.fd=sock;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock,&event) < 0){//注册事件
        perror("epoll_ctl");
        return 2;
    }
        
    for(; ;){
        struct epoll_event events[FD_SETSIZE];
        int size=epoll_wait(epoll_fd,events,FD_SETSIZE,-1);
        if(size < 0){
            perror("epoll_wait");
            continue;
        }
        if(0 == size){
            printf("epoll timeout\n");
            continue;
        }
        int i=0;
        for(;i<size;++i){
            if(events[i].data.fd == sock){
                printf("新链接\n");
                ProcessConnect(sock,epoll_fd);
                continue;
            }
            else{
                printf("处理数据\n");
                ProcessRequest(events[i].data.fd,epoll_fd);
                continue;
            }
        }
    }//for
    close(sock);
    //close(epoll_fd);
    return 0;
}
