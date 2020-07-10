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

#define false 0
#define true 1
#define BASE_DIR "bigfs/"
#define TCP_BUFFER_SIZE 2047
#define MAX_CLIENTS 20


int max(int a, int b){
    return (a>b)?a:b;
}

int min(int a, int b){
    return (a<b) ? a: b;
}


char *allocmem(int size){
    char *cstr = (char *)(calloc(size+1,sizeof(char)));
    return cstr;
}


char * trim (char *str) { // remove leading and trailing spaces
    str = strdup(str);
    if(str == NULL)
        return NULL;
    int begin = 0, end = strlen(str) -1, i;
    while (isspace (str[begin])){
        begin++;
    }

    if (str[begin] != '\0'){
        while (isspace (str[end]) || str[end] == '\n'){
            end--;
        }
    }
    str[end + 1] = '\0';

    return str+begin;
}

/*
 * Send the file on the the given fpath to the socket on sockfd
 * toWrite stores the size of the file to be sent
 */
int sendFile(int sockfd, char *fpath, long toWrite){
    int rfd = open(fpath,O_RDONLY);
    if(rfd == -1)
        return false;
    int val = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,val & (~O_NONBLOCK));

    sendfile(sockfd,rfd,0,toWrite);

    fcntl(sockfd,F_SETFL,val);

    return true;

}

/*
 * Get file from the socket on sockfd and store it on local storage
 */
int recvFile(int sockfd, long toRead,char *uid){
    //assuming sockfd is already set to non-blocking
    char *fpath = allocmem(30);
    strcat(fpath,BASE_DIR);
    strcat(fpath,uid);
    int wfd = open(fpath,O_WRONLY|O_CREAT,0666);
    if(wfd == -1)
        return false;
    long nread;
    int val = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,val & (~O_NONBLOCK));
    while(toRead > 0){
        char buf[TCP_BUFFER_SIZE+1] = {0};

        if((nread = read(sockfd,buf,min(toRead,TCP_BUFFER_SIZE))) < 0){
            perror("read");
        }
        else if(nread == 0){
            perror("EOF on socket");
            return false;
        }
        else{
            write(wfd,buf,nread);
            toRead -= nread;
        }
    }
    fcntl(sockfd,F_SETFL,val);
    close(wfd);
    printf("Received File - %s.\n\n",uid);
    return true;
}


int execute_cmd(int sockfd, char *cmd, int *p){
    char tmp = cmd[3];
    cmd[3] = '\0';
    if(strcmp(cmd,"UPL")==0){
        cmd[3] = tmp;
        long size;
        cmd = cmd+4;
        char uid[20];
        sscanf(cmd,"%ld%s",&size,uid);
        // printf("uid - %s\n",uid );
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            write(sockfd,uid,strlen(uid));
            printf("Adding new file %s\n",uid);
            if(fork() == 0){
                close(p[0]);
                if(!recvFile(sockfd,size,uid))
                    perror("Error Receiving file");
                char tbuf[20] = {0};
                sprintf(tbuf,"%d",sockfd);
                write(p[1],tbuf,strlen(tbuf));
                exit(0);
            }
            close(p[1]);
            return true;
        } else
            return false;
    }
    else if(strcmp(cmd,"DWN")==0){
        cmd[3] = tmp;
        char *uid = cmd+4;
        char *fpath = allocmem(strlen(uid) + 10);
        strcat(fpath,BASE_DIR);
        strcat(fpath,uid);

        int tmp_fd = open(fpath,O_RDONLY);
        if(tmp_fd == -1)
            return -1;
        long sz = lseek(tmp_fd,0,SEEK_END);
        close(tmp_fd);

        char buf[20] = {0};
        sprintf(buf,"%ld",sz);
        fd_set wset, rset;
        FD_ZERO(&wset);
        FD_SET(sockfd,&wset);
        select(sockfd+1,NULL,&wset,NULL,NULL);
        if(FD_ISSET(sockfd,&wset)){
            write(sockfd,buf,strlen(buf));
        }
        if(sz == -1)
            return false;
        FD_ZERO(&rset);
        FD_SET(sockfd,&rset);
        select(sockfd+1,&rset,NULL,NULL,NULL);
        if(FD_ISSET(sockfd,&rset)){
            int nr = read(sockfd,buf,19);
            buf[nr] = '\0';
            if(strcmp(buf,"READY")==0){
                printf("Sending file %s\n",uid);
                if(fork() == 0){
                    close(p[0]);
                    if(!sendFile(sockfd,fpath,sz))
                        perror("Error Sending file");
                    char tbuf[20] = {0};
                    sprintf(tbuf,"%d",sockfd);
                    write(p[1],tbuf,strlen(tbuf));
                    exit(0);
                }
                close(p[1]);
                return true;
            }
            else{
                return false;
            }

        }
        return false;

    }
    else if(strcmp(cmd,"REM")==0){
        cmd[3] = tmp;
        char *uid = cmd+4;
        char *fpath = allocmem(strlen(uid) + 10);
        strcat(fpath,BASE_DIR);
        strcat(fpath,uid);
        printf("Removing file %s\n",uid);
        if(fork() == 0){
            execlp("rm","rm",fpath,NULL);
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
        return 2;
    }
    else{
        cmd[3] = tmp;
        printf("Invalid Command.\n");
        perror(cmd);
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
        if(FD_ISSET(listenfd,&rset)){
            int clilen = sizeof(cliaddr);
            connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);
            
            //convertToNonBlocking
            int val = fcntl(connfd,F_GETFL,0);
            fcntl(connfd,F_SETFL,val|O_NONBLOCK);

            for(i=0;i<MAX_CLIENTS;i++){
                if(client[i] == -1){
                    client[i] = connfd;
                    break;
                }
            }
            if(i == MAX_CLIENTS){
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
        for(i=0;i<=maxi;i++){

            if((sockfd = client[i]) == -1)
                continue;
            if(client[i] < 0){
                sockfd = -1 * client[i];    //this is a pipe
                if(FD_ISSET(sockfd,&rset)){
                    wait(NULL);
                    char buf[20] = {0};
                    if((nread = read(sockfd,buf,19)) <= 0){
                        perror("pipe");
                    }
                    close(sockfd);
                    int sfd;
                    sscanf(buf,"%d",&sfd);
                    client[i] = sfd;
                    FD_CLR(sockfd,&rset0);
                    FD_SET(sfd,&rset0);
                    if(--nready <= 0)
                        break;
                }
                continue;
            }
            if(FD_ISSET(sockfd,&rset)){
                //this socket is available for reading
                char buf[TCP_BUFFER_SIZE] = {0};
                if((nread = read(sockfd,buf,TCP_BUFFER_SIZE)) == 0){
                    close(sockfd);
                    printf("Closing Connection.\n");
                    FD_CLR(sockfd,&rset0);
                    client[i] = -1;
                    exit(0);
                }
                else{
                    int p[2];
                    pipe(p);
                    buf[nread] = '\0';
                    // printf("buf - %s\n",buf );
                    int ecret;
                    if((ecret=execute_cmd(sockfd,trim(buf),p))){
                        if(ecret != 2){
                            FD_CLR(sockfd,&rset0);
                            FD_SET(p[0],&rset0);
                            maxfd = max(maxfd,p[0]);
                            client[i] = (p[0]) * -1;
                        }
                        else{
                            close(p[0]);
                            close(p[1]);
                        }
                    }
                    else{
                        perror(buf);
                        printf("Error in executing command\n");
                    }
                }
                if(--nready <= 0)
                    break;
            }
        }

    }
}

int main(int argc, char *argv[]) {
    srand(time(0));
    int port;
    if(argc == 2)
        sscanf(argv[1],"%d",&port);
    else
        port = 8001;

    char *dir = BASE_DIR;
    struct stat sb;
    if(stat(dir,&sb)== 0 && S_ISDIR(sb.st_mode))
        printf("...");
    else{
        if(mkdir(dir,0777) == 0)
            printf("...");
        else{
            perror("mkdir");
            return false;
        }
    }
    runServer(port);

    return 0;
}