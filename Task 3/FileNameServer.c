#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<sys/sendfile.h>

#define BASE_DIR "bigfs/"
#define TCP_BUFFER_SIZE 2047
#define MAX_CLIENTS 20



char **strSplit(char *str, char tk){
    if(str == NULL || strcmp(str,"")==0)
        return NULL;
    char tkn[] = {tk};
    int sz = 0;

    for(int i=0;i<strlen(str);i++)
        if(str[i] == tk)
            sz++;
    sz+=2;

    char **arr = (char **)(calloc(sz,sizeof(char *)));
    char *tmpstr = strdup(str);
    int i = 0;
    char *token;
    while((token = strsep(&tmpstr,tkn)))
        arr[i++] = token;
    arr[i] = NULL;
    return arr;
}


int sendData(int sockfd, char *fpath, long toWrite){
    int rfd = open(fpath,O_RDONLY);
    if(rfd == -1)
        return 0;
    int val = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,val & (~O_NONBLOCK));

    sendfile(sockfd,rfd,0,toWrite);

    fcntl(sockfd,F_SETFL,val);
    printf("Sent data at %s\n",fpath);
    return 1;
}


int recvData(int sockfd, long toRead){
    //assuming sockfd is already set to non-blocking
    char *fpath;
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    long nread;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd,&read_fds);
    select(sockfd+1,&read_fds,NULL,NULL,&timeout);
    char buf[TCP_BUFFER_SIZE+1] = {0};
    if(FD_ISSET(sockfd,&read_fds)){
        if((nread = read(sockfd,buf,toRead)) < 0)
        {
            perror("read");
            exit(1);
        }
        else if(nread == 0)
        {
            perror("EOF on socket");
            return 0;
        }
        else
        {
            // printf("%s\n",buf );
            char **datarr = strSplit(buf,'#');
            fpath = datarr[0];

            int n = strlen(fpath);
            for(int i=0;i<n;i++)
            {
                if(fpath[i] == '/')
                {
                    fpath[i] = '\0';

                    struct stat sb;
                    if(stat(fpath,&sb)== 0 && S_ISDIR(sb.st_mode))
                    {}
                    else{
                        if(mkdir(fpath,0777) != 0)
                        {
                            perror("mkdir");
                            exit(1);
                        }
                    }

                    fpath[i] = '/';
                }
            }

            // printf("fpath - %s\n",fpath );
            int wfd = open(fpath,O_WRONLY|O_CREAT,0666);
            if(wfd == -1)
            {
                perror("open");
                exit(1);
            }
            char *wbuf = buf+1+strlen(fpath);
            write(wfd,wbuf,strlen(wbuf));
            close(wfd);
            printf("Added new data at %s\n",fpath);
        }
    }
    else{
        perror("Timeout.");
        exit(1);
    }
    return 1;
}

int execute_cmd(int sockfd, char *cmd){
    char tmp = cmd[3];
    cmd[3] = '\0';
    if(strcmp(cmd,"UPL") == 0)
    {
        cmd[3] = tmp;
        long size;
        cmd = cmd+4;
        sscanf(cmd,"%ld",&size);

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset))
        {
            char tbuf[] = "READY";
            write(sockfd,tbuf,strlen(tbuf));
            if(!recvData(sockfd,size))
            {
                perror("Error Receiving data.");
                exit(1);
            }
            return 1;
        }
        else
            return 0;
    }
    else if(strcmp(cmd,"DWN") == 0)
    {
        cmd[3] = tmp;
        char *fpath = cmd+4;

        int tmp_fd = open(fpath,O_RDONLY);
        if(tmp_fd == -1)
        {
            perror("Error in opening file");
            exit(1);
        }
        long sz = lseek(tmp_fd,0,SEEK_END);
        close(tmp_fd);

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            if(sz == -1)
            {
                char buf[20] = {0};
                sprintf(buf,"%ld",sz);
                if(write(sockfd,buf,strlen(buf)) == 0)
                    perror("write");
            }
            else if(!sendData(sockfd,fpath,sz))
            {
                perror("Error in Sending data.");
                exit(1);
            }
        }
        if(sz == -1)
            return 0;
        else
            return 1;

    }
    else if(strcmp(cmd,"REM") == 0)
    {
        cmd[3] = tmp;
        char *fpath = cmd+4;

        if(fork() == 0)
        {
            execlp("rm","rm","-r",fpath,NULL);
            perror("execlp");
            exit(0);
        }
        wait(NULL);

        char buf[20] = {0};
        strcpy(buf,"DONE");
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            write(sockfd,buf,strlen(buf));
        }
        printf("Removed entries at %s\n",fpath);
        return 1;
    }
    else if(strcmp(cmd,"MOV") == 0)
    {
        cmd[3] = tmp;
        char **cmdarr = strSplit(cmd,' ');
        char *srcpath = cmdarr[1];
        char *dstpath = cmdarr[2];
        if(fork() == 0)
        {
            execlp("mv","mv",srcpath,dstpath,NULL);
            perror("execlp");
            exit(0);
        }
        wait(NULL);

        char buf[20] = {0};
        strcpy(buf,"DONE");
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            write(sockfd,buf,strlen(buf));
        }
        printf("Moved entries at %s to %s\n",srcpath,dstpath);
        return 1;
    }
    else if(strcmp(cmd,"LS ") == 0)
    {
        cmd[3] = tmp;
        char *fpath = cmd+3;
        int p[2];
        pipe(p);
        if(fork() == 0){
            close(1);
            close(p[0]);
            dup2(p[1],1);
            execlp("ls","ls","-F",fpath,NULL);
            perror("execlp");
            exit(0);
        }
        close(p[1]);
        wait(NULL);

        char buf[TCP_BUFFER_SIZE] = {0};
        int nr = read(p[0],buf,TCP_BUFFER_SIZE);
        if(nr == 0){
            sprintf(buf,"-1");
            nr = 2;
        }

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            write(sockfd,buf,nr);
        }
        printf("Sent ls data for %s\n",fpath);
        return 1;
    }
    else{
        cmd[3] = tmp;
        printf("Invalid Command.\n");
        perror(cmd);
        exit(0);
    }
}

void runServer(int port){

    int i,maxi,maxfd,listenfd,nready,connfd,sockfd,nread;
    int client[MAX_CLIENTS];
    fd_set rset,rset0;

    struct sockaddr_in cliaddr, servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    bind(listenfd,(struct sockaddr*) &servaddr, sizeof(servaddr));
    listen(listenfd,MAX_CLIENTS);
    printf("Server running on port %d\n",port);


    maxfd = listenfd;
    maxi = -1;
    for(i=0; i<MAX_CLIENTS;i++)
        client[i] = -1;
    FD_ZERO(&rset0);
    FD_SET(listenfd,&rset0);
    
    int loopover = 0;
    while(!loopover){
        rset = rset0;
        nready = select(maxfd+1,&rset,NULL,NULL,NULL);
        if(FD_ISSET(listenfd,&rset))
        {
            int clilen = sizeof(cliaddr);
            connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);

            // Convert to Non-Blocking
            int val = fcntl(connfd,F_GETFL,0);
            fcntl(connfd,F_SETFL,val|O_NONBLOCK);

            for(i=0;i<MAX_CLIENTS;i++)
            {
                if(client[i] == -1)
                {
                    client[i] = connfd;
                    break;
                }
            }

            if(i == MAX_CLIENTS)
            {
                perror("Too Many Clients.\n");
                close(connfd);
            }

            printf("Connected to client %s\n",inet_ntoa(cliaddr.sin_addr));
            FD_SET(connfd,&rset0);
            if(connfd > maxfd)
                maxfd = connfd;
            if(i > maxi)
                maxi = i;
            if(--nready <= 0)
                continue;
        }
        for(i=0;i<=maxi;i++)
        {

            if((sockfd = client[i]) == -1)
                continue;

            if(FD_ISSET(sockfd,&rset))
            {
                char buf[TCP_BUFFER_SIZE] = {0};
                if((nread = read(sockfd,buf,TCP_BUFFER_SIZE)) == 0)
                {
                    close(sockfd);
                    printf("Closing Connection.\n");
                    FD_CLR(sockfd,&rset0);
                    client[i] = -1;
                    exit(0);
                }
                else
                {
                    buf[nread] = '\0';
                    if(!execute_cmd(sockfd,buf))
                    {
                        perror("Error in executing command\n");
                        exit(1);
                    }
                }
                if(--nready <= 0)
                    break;
            }
        }

    }
}

int main(int argc, char *argv[]) {
    int port;
    if(argc == 2)
        sscanf(argv[1],"%d",&port);
    else
        port = 8000;
    
    char *dir = BASE_DIR;
    struct stat sb;
    if(stat(dir,&sb)== 0 && S_ISDIR(sb.st_mode))
    {}
    else
    {
        if(mkdir(dir,0777) != 0)
        {
            perror("mkdir");
            exit(1);
        }        
    }

    runServer(port);

    return 0;
}