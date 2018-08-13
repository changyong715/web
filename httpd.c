/*************************************************************************
	> File Name: httpd.c
	> Author: 
	> Mail: 
	> Created Time: Sun 12 Aug 2018 04:16:30 AM PDT
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>

#define MAX 1024

int startup(int port)
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
}

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
    char line[MAX];
    do{
        GetLine(sock,line,sizeof(line));
    }while(strcmp(line,"\n"));
}

void* handlerRequest(void* arg)
{
    printf("get a new client\n");
    int sock=(int)arg;   
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

    }
    else if(0 == strcasecmp(method,"POST"))
    {
    
    }
    else//清理报头(处理完请求再给响应)
    {
        ClearHeader(sock);
        statuscode=404;
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
    printf("method:%s,url:%s,query_string:%s\n",method,path,query_string);

end:
    if(statuscode != 200)
    {
       // EchoError(statuscode);
    }
    close(sock);
}

int main(int argc,char* argv[])
{
    if(argc!=2){
        printf("Usage: %s port\n",argv[0]);
        return 1;
    }

    int listen_sock=startup(atoi(argv[1]));

    while(1){
        struct sockaddr_in client;
        socklen_t len=sizeof(client);
        int sock=accept(listen_sock,(struct sockaddr*)&client,&len);
        if(sock < 0){
            perror("accept");
            continue;
        }
        
        pthread_t tid;
        pthread_create(&tid,NULL,handlerRequest,(void*)sock);
        pthread_detach(tid);//分离主线程，防止阻塞

    }//while

    return 0;
}
