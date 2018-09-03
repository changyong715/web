/*************************************************************************
	> File Name: test_cgi.c
	> Author: 
	> Mail: 
	> Created Time: Sun 19 Aug 2018 01:36:21 AM PDT
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int Handle()
{
    char buf[1024];
    int len=-1;
    if(getenv("METHOD"))
    {
        if(strcasecmp(getenv("METHOD"),"GET") == 0)
        {
            strcpy(buf,getenv("QUERY_STRING"));
        }
        else
        {
            len=atoi(getenv("CONTENT_LENGTH"));
            int i=0;
            for(;i<len;i++)
            {
                read(0,buf+i,1);
            }
            buf[i]=0;
        }
    }

    char *username="pangdun";
    char *password="123456";
    char x[1024];
    char y[1024];
    char ret[1024];

    sscanf(buf,"x=%s",ret);
    int count=strlen(ret);
    int i=0;
    for(;i<count;++i){
        if(ret[i] == '&')
            break;
        *(x+i)=ret[i];
    }
    *(x+i)=0;

    int j=0;
    for(i+=3;i<count;++i){
        *(y+j)=ret[i];
        j+=1;
    }
    *(y+j)='\0';

    int s1=strcmp(username,x); 
    int s2=strcmp(y,password);
    fflush(stdout);
    if(0 == s1 && 0 == s2)
        return 0;
    else
        return 1;
}
int main()
{
    int result=Handle();
    
    if(result == 0)
    {
        printf("<html>");
        printf("<h1>OK</h1>\n");
        printf("<html>");
    }
    else
    {
         printf("<html>");
         printf("<h1>ERROR</h1>\n");
         printf("<html>");
    }
    return 0;
}
